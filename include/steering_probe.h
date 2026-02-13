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
		float valleySlideStrength = 50.0f; // How hard the terrain pushes into the valley
		float flyHeight = 30.0f;           // Target height above ground

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
			// Assuming we can access the same params as terrain_generator.cpp:
			glm::vec3 noise = terrain->GetPathData(position.x, position.z);

			float     distFromSpine = noise.x;                // Signed distance! (- is left, + is right)
			glm::vec2 gradient = glm::normalize(glm::vec2(noise.y, noise.z)) * 2.0f; // Points UPHILL (away from spine)

			// Force A: Valley Slide
			// If dist is positive, gradient points away -> we want -gradient.
			// If dist is negative, gradient points away (from negative peak) -> we want +gradient.
			// Math: -gradient * dist pushes us towards 0.
			glm::vec2 slideForce2D = -gradient * distFromSpine * valleySlideStrength;

			// Force B: Flow Alignment (Optional but recommended)
			// Helps the probe carry momentum through corners
			glm::vec2 valleyDir = glm::vec2(-gradient.y, gradient.x); // Perpendicular
			// Flip if pointing against the player
			if (glm::dot(valleyDir, glm::vec2(playerVel.x, playerVel.z)) < 0) {
				valleyDir = -valleyDir;
			}
			glm::vec2 flowForce2D = valleyDir * (glm::length(velocity) * 0.5f);

			// --- 4. HEIGHT CONTROL ---
			// Sample actual ground height to stay above it
			// (Using your existing pointGenerate or point query)
			float     height = terrain->GetDistanceAboveTerrain(position);
			float     liftForce = (flyHeight - height) * 10.0f;

			// --- 5. INTEGRATION ---
			glm::vec3 totalForce = tetherForce;
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