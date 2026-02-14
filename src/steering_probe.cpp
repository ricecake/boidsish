#include "steering_probe.h"

#include <algorithm>

#include "checkpoint_ring.h"
#include "constants.h"
#include "entity.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	SteeringProbe::SteeringProbe(std::shared_ptr<ITerrainGenerator> terrain): terrain_(terrain) {}

	void SteeringProbe::Update(float dt, const glm::vec3& playerPos, const glm::vec3& playerVel) {
		if (!terrain_)
			return;

		// --- 1. THE LURE (Where we want to be) ---
		float     speed = glm::length(playerVel);
		float     lookAheadTime = std::clamp(speed * 0.1f, 3.0f, 5.0f);
		glm::vec3 lurePos = playerPos + (playerVel * lookAheadTime);

		// --- 2. TETHER FORCE (The Leash) ---
		glm::vec3 displacement = lurePos - position_;
		glm::vec3 tetherForce = displacement * springStiffness_;

		// --- 3. TERRAIN FORCES (The Valley) ---
		glm::vec3 noise = terrain_->GetPathData(position_.x, position_.z);
		float     distFromSpine = noise.x;
		glm::vec2 gradient = glm::normalize(glm::vec2(noise.y, noise.z)) * 2.0f;

		glm::vec2 slideForce2D = -gradient * distFromSpine * valleySlideStrength_;

		glm::vec2 valleyDir = glm::vec2(-gradient.y, gradient.x);
		glm::vec2 travelDir = (glm::length(playerVel) > 0.1f) ? glm::normalize(glm::vec2(playerVel.x, playerVel.z))
															  : glm::vec2(0.0f, -1.0f);
		if (glm::dot(valleyDir, travelDir) < 0) {
			valleyDir = -valleyDir;
		}
		glm::vec2 flowForce2D = valleyDir * (glm::length(velocity_) * 1.5f);

		// --- 4. OBSTACLE AVOIDANCE (5-RAY EYES) ---
		glm::vec3 currentDir = (glm::length(velocity_) > 0.1f) ? glm::normalize(velocity_) : glm::vec3(0, 0, -1);
		glm::vec3 right = glm::cross(currentDir, glm::vec3(0, 1, 0));
		if (glm::length(right) < 0.001f)
			right = glm::vec3(1, 0, 0);
		else
			right = glm::normalize(right);
		glm::vec3 up = glm::normalize(glm::cross(right, currentDir));

		struct Ray {
			glm::vec3 dir;
			float     weight;
		};

		float            spread = 0.4f;
		std::vector<Ray> rays = {
			{currentDir, 1.5f},                                      // Center
			{glm::normalize(currentDir - right * spread), 1.0f},     // Left
			{glm::normalize(currentDir + right * spread), 1.0f},     // Right
			{glm::normalize(currentDir + up * spread * 0.5f), 1.2f}, // Up (biased to look for cliffs)
			{glm::normalize(currentDir - up * spread * 0.5f), 0.8f}  // Down
		};

		glm::vec3 avoidanceForce(0.0f);
		for (const auto& ray : rays) {
			float     hitDist;
			glm::vec3 hitNormal;
			if (terrain_->RaycastCached(position_, ray.dir, avoidanceLookAhead_, hitDist, hitNormal)) {
				float forceMag = (1.0f - (hitDist / avoidanceLookAhead_)) * avoidanceStrength_;
				// Push away from the surface normal
				avoidanceForce += hitNormal * forceMag * ray.weight;

				// If it's the forward ray, also add a significant upward push if we're hitting a wall
				if (&ray == &rays[0] && hitNormal.y < 0.5f) {
					avoidanceForce.y += avoidanceStrength_ * 2.0f;
				}
			}
		}

		// --- 5. HEIGHT CONTROL ---
		float heightAbove = terrain_->GetDistanceAboveTerrain(position_);
		float liftForce = (flyHeight_ - heightAbove) * 15.0f;
		if (heightAbove < 0.0f)
			liftForce += 100.0f; // Stronger push if below ground

		// --- 6. NORTH BIAS ---
		glm::vec3 northBiasForce(0.0f, 0.0f, -northBiasStrength_);

		// --- 7. INTEGRATION ---
		glm::vec3 totalForce = tetherForce + avoidanceForce + northBiasForce;
		totalForce.x += slideForce2D.x + flowForce2D.x;
		totalForce.z += slideForce2D.y + flowForce2D.y;
		totalForce.y += liftForce;

		glm::vec3 acceleration = totalForce / mass_;
		velocity_ += acceleration * dt;
		velocity_ *= drag_;
		position_ += velocity_ * dt;
	}

	void SteeringProbe::HandleCheckpoints(float dt, EntityHandler& handler, std::shared_ptr<EntityBase> player) {
		timeSinceLastDrop_ += dt;

		if (glm::length(velocity_) < 0.1f)
			return;
		glm::vec3 currentDir = glm::normalize(velocity_);

		float dist = glm::distance(position_, lastCheckpointPos_);
		float dot = glm::dot(currentDir, lastCheckpointDir_);

		// Thresholds for dropping checkpoints
		bool timeTrigger = timeSinceLastDrop_ > 5.0f && dist > 150.0f;
		bool distTrigger = dist > 400.0f;
		bool turnTrigger = (dot < 0.85f) && (dist > 150.0f); // ~25 degrees

		if (timeTrigger || distTrigger || turnTrigger) {
			float           radius = 25.0f;
			CheckpointStyle style = CheckpointStyle::SILVER;

			glm::vec3 up = glm::vec3(0, 1, 0);
			if (std::abs(glm::dot(currentDir, up)) > 0.99f)
				up = glm::vec3(1, 0, 0);

			// Ring faces BACKWARDS so player coming from behind goes through it
			glm::quat rotation = glm::quatLookAt(currentDir, up);

			auto id = handler
						  .AddEntity<CheckpointRing>(radius, style, [&handler](float d, std::shared_ptr<EntityBase> e) {
							  // Score could be added here if we had access to the handler or score indicator
							  (void)d;
							  (void)e;
							  logger::LOG("GOT RING PASS");
						  });

			auto ring = handler.GetEntity(id);
			if (ring) {
				ring->SetPosition(position_.x, position_.y + radius, position_.z);
				auto checkpoint = std::dynamic_pointer_cast<CheckpointRing>(ring);
				if (checkpoint) {
					checkpoint->SetOrientation(rotation);
					if (player) {
						checkpoint->RegisterEntity(player);
					}
					checkpoint->UpdateShape();
				}
			}

			lastCheckpointPos_ = position_;
			lastCheckpointDir_ = currentDir;
			timeSinceLastDrop_ = 0.0f;
		}
	}

} // namespace Boidsish
