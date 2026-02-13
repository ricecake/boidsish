#pragma once

#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "Simplex.h"
#include "constants.h"
#include "terrain.h"
#include "terrain_deformation_manager.h"
#include "terrain_generator.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include "thread_pool.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

	struct SteeringProbe {
		glm::vec3                         position;
		glm::vec3                         velocity = glm::vec3(0.0f);
		std::shared_ptr<ITerrainGenerator> terrain;

		// Config: "Leash" physics
		float mass = 2.00f;
		float drag = 0.95f;           // Air resistance (prevents orbiting)
		float springStiffness = 0.50f; // How hard the leash pulls

		// Config: Terrain physics
		float valleySlideStrength = 60.0f; // How hard the terrain pushes into the valley
		float flyHeight = 30.0f;           // Target height above ground

		// Config: North bias
		float northBiasStrength = 15.0f; // Constant pull towards North (-Z)

		// Config: Obstacle avoidance
		float avoidanceLookAhead = 60.0f;
		float avoidanceRadius = 25.0f;
		float avoidanceStrength = 20.0f;

		// State for dropping checkpoints
		glm::vec3 lastCheckpointPos;
		glm::vec3 lastCheckpointDir;
		float     timeSinceLastDrop = 0.0f;

		void Update(
			float                   dt,
			const glm::vec3&        playerPos,
			const glm::vec3&        playerVel
			// std::vector<glm::vec3>& outCheckpoints
		) {
			// --- 1. THE LURE (Where we want to be) ---
			// Look ahead 2-3 seconds. Dynamic based on speed.
			float     speed = glm::length(playerVel);
			float     lookAheadTime = std::clamp(speed * 0.1f, 3.0f, 5.0f);
			glm::vec3 lurePos = playerPos + (playerVel * lookAheadTime);

			// --- 2. TETHER FORCE (The Leash) ---
			// Pulls the probe towards the lure position
			glm::vec3 displacement = lurePos - position;
			glm::vec3 tetherForce = displacement * springStiffness;

			// --- 3. TERRAIN FORCES (The Valley) ---
			// We need the raw noise to know if we are on the left or right bank.
			glm::vec3 noise = terrain->GetPathData(position.x, position.z);

			float     distFromSpine = noise.x;                // Signed distance! (- is left, + is right)
			glm::vec2 gradient = glm::normalize(glm::vec2(noise.y, noise.z)) * 2.0f; // Points UPHILL (away from spine)

			// Force A: Valley Slide
			// Math: -gradient * dist pushes us towards 0 (the spine of the path).
			glm::vec2 slideForce2D = -gradient * distFromSpine * valleySlideStrength;

			// Force B: Flow Alignment
			// Helps the probe carry momentum through corners
			glm::vec2 valleyDir = glm::vec2(-gradient.y, gradient.x); // Perpendicular
			// Flip if pointing against the general travel direction (North) or player direction
			glm::vec2 travelDir = (glm::length(playerVel) > 0.1f) ? glm::normalize(glm::vec2(playerVel.x, playerVel.z)) : glm::vec2(0.0f, -1.0f);
			if (glm::dot(valleyDir, travelDir) < 0) {
				valleyDir = -valleyDir;
			}
			glm::vec2 flowForce2D = valleyDir * (glm::length(velocity) * 1.5f);

			// --- 4. OBSTACLE AVOIDANCE (EYES) ---
			// We look ahead to anticipate terrain changes
			glm::vec3 currentDir = (glm::length(velocity) > 0.1f) ? glm::normalize(velocity) : glm::vec3(0, 0, -1);
			auto getAvoidanceForce = [&](glm::vec3 probePos, float weight) {
				float distAbove = terrain->GetDistanceAboveTerrain(probePos);
				if (distAbove < flyHeight) {
					auto [h, normal] = terrain->GetTerrainPropertiesAtPoint(probePos.x, probePos.z);
					// Push away from the terrain normal proportional to how deep we are
					// We also add a vertical boost to clear the obstacle
					float pushMag = (flyHeight - distAbove) * avoidanceStrength * weight;
					return (normal + glm::vec3(0, 1, 0)) * pushMag;
				}
				return glm::vec3(0.0f);
			};

			// Probes: Forward, Mid-Left, Mid-Right, Close-Left, Close-Right
			glm::vec3 avoidForce = getAvoidanceForce(position + currentDir * avoidanceLookAhead, 1.0f);

			glm::vec3 right = (std::abs(currentDir.y) > 0.99f) ? glm::vec3(1, 0, 0) : glm::normalize(glm::cross(currentDir, glm::vec3(0, 1, 0)));

			// Mid-range side probes
			avoidForce += getAvoidanceForce(position + currentDir * (avoidanceLookAhead * 0.6f) - right * avoidanceRadius, 0.8f);
			avoidForce += getAvoidanceForce(position + currentDir * (avoidanceLookAhead * 0.6f) + right * avoidanceRadius, 0.8f);

			// Close-range wide probes
			avoidForce += getAvoidanceForce(position + currentDir * (avoidanceLookAhead * 0.3f) - right * (avoidanceRadius * 1.5f), 1.2f);
			avoidForce += getAvoidanceForce(position + currentDir * (avoidanceLookAhead * 0.3f) + right * (avoidanceRadius * 1.5f), 1.2f);

			// --- 5. HEIGHT CONTROL ---
			float     height = terrain->GetDistanceAboveTerrain(position);
			float     liftForce = (flyHeight - height) * 10.0f;

			// --- 6. NORTH BIAS ---
			glm::vec3 northBiasForce(0.0f, 0.0f, -northBiasStrength);

			// --- 7. INTEGRATION ---
			glm::vec3 totalForce = tetherForce + avoidForce + northBiasForce;
			totalForce.x += slideForce2D.x + flowForce2D.x;
			totalForce.z += slideForce2D.y + flowForce2D.y;
			totalForce.y += liftForce;

			glm::vec3 acceleration = totalForce / mass;
			velocity += acceleration * dt;
			velocity *= drag; // Dampening
			position += velocity * dt;

			// --- 6. CHECKPOINT DROPPING ---
			// HandleCheckpoints(dt, outCheckpoints);
		}

		void HandleCheckpoints(float dt, std::vector<glm::vec3>& outPoints) {
			timeSinceLastDrop += dt;

			glm::vec3 currentDir = glm::normalize(velocity);

			// Logic: Drop if 5 seconds passed OR we turned > 15 degrees
			bool  timeTrigger = timeSinceLastDrop > 5.0f;
			float dot = glm::dot(currentDir, lastCheckpointDir);
			bool  turnTrigger = (dot < 0.96f) && (timeSinceLastDrop > 0.5f); // ~15 degrees

			if (timeTrigger || turnTrigger) {
				outPoints.push_back(position);
				lastCheckpointPos = position;
				lastCheckpointDir = currentDir;
				timeSinceLastDrop = 0.0f;
			}
		}
	};

}; // namespace Boidsish