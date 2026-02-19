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
				avoidanceForce += -ray.dir * forceMag * ray.weight;

				// If it's the forward ray, also add a significant upward push if we're hitting a wall
				if (&ray == &rays[0] && hitNormal.y < 0.5f) {
					avoidanceForce += -ray.dir * 2.0f;
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

	void SteeringProbe::HandleCheckpoints(float dt, const EntityHandler& handler, std::shared_ptr<EntityBase> player) {
		timeSinceLastDrop_ += dt;

		if (player) {
			// 1. Prune checkpoints if the probe backtracks or the path becomes too sharp.
			// We iterate backwards from the most recently placed checkpoint.
			while (!activeCheckpoints_.empty()) {
				int  lastId = activeCheckpoints_.back();
				auto lastRing = handler.GetEntity(lastId);
				if (!lastRing) {
					activeCheckpoints_.pop_back();
					continue;
				}

				glm::vec3 cpPos = lastRing->GetPosition().Toglm();
				auto      checkpoint = std::dynamic_pointer_cast<CheckpointRing>(lastRing);
				if (checkpoint) {
					// Adjust for the vertical offset (ring center is radius above ground)
					auto shape = std::dynamic_pointer_cast<CheckpointRingShape>(checkpoint->GetShape());
					if (shape)
						cpPos.y -= shape->GetRadius();
				}

				// Identify the "previous" position in the chain to define the segment leading INTO this checkpoint.
				glm::vec3 prevPos;
				if (activeCheckpoints_.size() > 1) {
					auto prevRing = handler.GetEntity(activeCheckpoints_[activeCheckpoints_.size() - 2]);
					if (prevRing) {
						prevPos = prevRing->GetPosition().Toglm();
						auto pCheck = std::dynamic_pointer_cast<CheckpointRing>(prevRing);
						if (pCheck) {
							auto pShape = std::dynamic_pointer_cast<CheckpointRingShape>(pCheck->GetShape());
							if (pShape)
								prevPos.y -= pShape->GetRadius();
						}
					} else {
						prevPos = player->GetPosition().Toglm();
					}
				} else {
					prevPos = player->GetPosition().Toglm();
				}

				glm::vec3 inSegment = cpPos - prevPos;
				glm::vec3 outSegment = position_ - cpPos;

				// Use 2D directions for angle check to be more robust against vertical bobbing
				glm::vec2 inDir2D(inSegment.x, inSegment.z);
				glm::vec2 outDir2D(outSegment.x, outSegment.z);

				if (glm::length(inDir2D) > 0.1f)
					inDir2D = glm::normalize(inDir2D);
				else {
					glm::vec3 pVel = player->GetVelocity().Toglm();
					inDir2D = (glm::length(glm::vec2(pVel.x, pVel.z)) > 0.1f)
						? glm::normalize(glm::vec2(pVel.x, pVel.z))
						: glm::vec2(0, -1);
				}

				bool shouldPrune = false;

				// Only prune if we have moved a significant distance from the checkpoint,
				// and the angle is sharp OR we have clearly backtracked.
				float distToCP = glm::length(outSegment);
				if (distToCP > 60.0f) {
					if (glm::length(outDir2D) > 0.1f) {
						float dot = glm::dot(inDir2D, glm::normalize(outDir2D));
						// Angle > 70 degrees => cos(70) ~ 0.342
						if (dot < 0.342f) {
							shouldPrune = true;
						}
					}
				} else {
					// Check if we've backtracked past the plane of the checkpoint
					if (glm::dot(outSegment, glm::vec3(inDir2D.x, 0, inDir2D.y)) < -20.0f) {
						shouldPrune = true;
					}
				}

				if (shouldPrune) {
					auto ring = std::dynamic_pointer_cast<CheckpointRing>(handler.GetEntity(lastId));
					if (ring) {
						ring->SetStatus(CheckpointStatus::PRUNED);
					}
					handler.QueueRemoveEntity(lastId);
					activeCheckpoints_.pop_back();

					// Update tracker state so we don't immediately drop a new one in a bad spot
					if (activeCheckpoints_.empty()) {
						lastCheckpointPos_ = player->GetPosition().Toglm();
						glm::vec3 pVel = player->GetVelocity().Toglm();
						lastCheckpointDir_ = (glm::length(pVel) > 0.1f) ? glm::normalize(pVel) : glm::vec3(0, 0, -1);
					} else {
						auto newLast = handler.GetEntity(activeCheckpoints_.back());
						if (newLast) {
							lastCheckpointPos_ = newLast->GetPosition().Toglm();
							auto pCheck = std::dynamic_pointer_cast<CheckpointRing>(newLast);
							if (pCheck) {
								auto pShape = std::dynamic_pointer_cast<CheckpointRingShape>(pCheck->GetShape());
								if (pShape)
									lastCheckpointPos_.y -= pShape->GetRadius();
							}

							glm::vec3 pPos;
							if (activeCheckpoints_.size() > 1) {
								auto pRing = handler.GetEntity(activeCheckpoints_[activeCheckpoints_.size() - 2]);
								if (pRing) {
									pPos = pRing->GetPosition().Toglm();
									auto ppCheck = std::dynamic_pointer_cast<CheckpointRing>(pRing);
									if (ppCheck) {
										auto ppShape = std::dynamic_pointer_cast<CheckpointRingShape>(
											ppCheck->GetShape()
										);
										if (ppShape)
											pPos.y -= ppShape->GetRadius();
									}
								} else {
									pPos = player->GetPosition().Toglm();
								}
							} else {
								pPos = player->GetPosition().Toglm();
							}
							lastCheckpointDir_ = (glm::length(lastCheckpointPos_ - pPos) > 0.1f)
								? glm::normalize(lastCheckpointPos_ - pPos)
								: glm::vec3(inDir2D.x, 0, inDir2D.y);
						}
					}
				} else {
					break; // This one is fine
				}
			}

			// 2. Clean up "way off course" checkpoints
			for (auto it = activeCheckpoints_.begin(); it != activeCheckpoints_.end();) {
				auto ring_base = handler.GetEntity(*it);
				if (!ring_base) {
					it = activeCheckpoints_.erase(it);
					continue;
				}

				float dist = glm::distance(player->GetPosition().Toglm(), ring_base->GetPosition().Toglm());
				if (dist > Constants::Project::Camera::DefaultFarPlane()) {
					auto ring = std::dynamic_pointer_cast<CheckpointRing>(ring_base);
					if (ring) {
						ring->SetStatus(CheckpointStatus::OUT_OF_RANGE);
					}
					handler.QueueRemoveEntity(*it);
					it = activeCheckpoints_.erase(it);
					continue;
				}
				++it;
			}
		}

		if (glm::length(velocity_) < 0.1f)
			return;
		glm::vec3 currentDir = glm::normalize(velocity_);

		float dist = glm::distance(position_, lastCheckpointPos_);
		float dot = glm::dot(currentDir, lastCheckpointDir_);

		// Thresholds for dropping checkpoints
		bool timeTrigger = timeSinceLastDrop_ > 5.0f && dist > 150.0f;
		bool distTrigger = dist > 400.0f;
		bool turnTrigger = (dot < 0.90f) && (dist > 100.0f); // ~25 degrees

		if (timeTrigger || distTrigger || turnTrigger) {
			float           radius = 25.0f;
			CheckpointStyle style = CheckpointStyle::GOLD;

			glm::vec3 up = glm::vec3(0, 1, 0);
			if (std::abs(glm::dot(currentDir, up)) > 0.99f)
				up = glm::vec3(1, 0, 0);

			// Ring faces BACKWARDS so player coming from behind goes through it
			glm::quat rotation = glm::quatLookAt(currentDir, up);

			int  id = handler.GetNextId();
			auto callback = [](float d, std::shared_ptr<EntityBase> e) {
				(void)d;
				(void)e;
				logger::LOG("GOT RING PASS");
			};

			glm::vec3 spawn_pos = position_;
			spawn_pos.y += radius;

			handler.QueueAddEntityWithId<CheckpointRing>(
				id,
				radius,
				style,
				callback,
				spawn_pos,
				rotation,
				player,
				next_sequence_id_++
			);

			activeCheckpoints_.push_back(id);

			lastCheckpointPos_ = position_;
			lastCheckpointDir_ = currentDir;
			timeSinceLastDrop_ = 0.0f;
		}
	}

} // namespace Boidsish
