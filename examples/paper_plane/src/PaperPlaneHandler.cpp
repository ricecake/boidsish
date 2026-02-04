#include "PaperPlaneHandler.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "GuidedMissileLauncher.h"
#include "PaperPlane.h"
#include "VortexFlockingEntity.h"
#include "constants.h"
#include "graphics.h"
#include "hud.h"
#include "neighbor_utils.h"
#include "terrain_generator.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Define the global weapon selection variable here
	int selected_weapon = 0;

	PaperPlaneHandler::PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool):
		SpatialEntityHandler(thread_pool), eng_(rd_()), damage_dist_(0.25f, 0.5f) {
		// std::uniform_real_distribution<> dis_pos(-40.0, 40.0);
		// std::uniform_real_distribution<> dis_y(30.0, 90.0);
		// std::uniform_real_distribution<> dis_vel(-5.0, 5.0);

		// for (int i = 0; i < 100; ++i) {
		// 	auto entity = std::make_shared<VortexFlockingEntity>(i);
		// 	entity->SetPosition(dis_pos(eng_), dis_y(eng_), dis_pos(eng_));
		// 	entity->SetVelocity(dis_vel(eng_), dis_vel(eng_), dis_vel(eng_));
		// 	AddEntity(i, entity);
		// }
	}

	void PaperPlaneHandler::RecordTarget(std::shared_ptr<GuidedMissileLauncher> target) const {
		if (target) {
			std::lock_guard<std::mutex> lock(target_mutex_);
			target_counts_[target->GetId()]++;
		}
	}

	int PaperPlaneHandler::GetTargetCount(std::shared_ptr<GuidedMissileLauncher> target) const {
		if (target) {
			std::lock_guard<std::mutex> lock(target_mutex_);
			auto                        it = target_counts_.find(target->GetId());
			if (it != target_counts_.end()) {
				return it->second;
			}
		}
		return 0;
	}

	void PaperPlaneHandler::PreparePlane(std::shared_ptr<PaperPlane> plane) {
		if (!plane || !vis)
			return;

		float     best_score = -1e10f;
		glm::vec3 best_pos(0, 100, 0);
		glm::vec3 best_dir(0, 0, -1);

		// Sample a grid around the starting area
		for (float x = -300; x <= 300; x += 30) {
			for (float z = -300; z <= 300; z += 30) {
				auto [h, norm] = vis->GetTerrainPointPropertiesThreadSafe(x, z);
				if (h < 15.0f)
					continue; // Avoid valleys/water

				// Test 8 directions
				for (int i = 0; i < 8; ++i) {
					float     angle = i * (glm::pi<float>() / 4.0f);
					glm::vec3 dir(sin(angle), 0, cos(angle));

					// Look ahead to check gradient
					float check_dist = 60.0f;
					auto [h_ahead, norm_ahead] = vis->GetTerrainPointPropertiesThreadSafe(
						x + dir.x * check_dist,
						z + dir.z * check_dist
					);

					float gradient = h - h_ahead; // positive is downslope
					float score = h * 0.5f + gradient * 2.0f;

					// Penalize if pointing directly into a steep uphill
					if (gradient < -10.0f)
						score -= 100.0f;

					if (score > best_score) {
						best_score = score;
						best_pos = glm::vec3(x, h + 60.0f, z);
						best_dir = dir;
					}
				}
			}
		}

		plane->SetPosition(best_pos.x, best_pos.y, best_pos.z);
		glm::quat orient = glm::quatLookAt(glm::normalize(best_dir), glm::vec3(0, 1, 0));
		plane->SetOrientation(orient);
		plane->SetVelocity(best_dir * 40.0f); // Set a good starting speed
		plane->UpdateShape();

		// Update camera to follow
		vis->GetCamera().x = best_pos.x;
		vis->GetCamera().y = best_pos.y + 5;
		vis->GetCamera().z = best_pos.z + 10;
	}

	void PaperPlaneHandler::PreTimestep(float time, float delta_time) {
		{
			std::lock_guard<std::mutex> lock(target_mutex_);
			target_counts_.clear();
		}
		if (damage_timer_ > 0.0f) {
			damage_timer_ -= delta_time;
			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", false);
				// vis->TogglePostProcessingEffect("Time Stutter", false);
			}
		}

		if (vis && vis->GetTerrainGenerator()) {
			const auto               visible_chunks = vis->GetTerrainGenerator()->getVisibleChunksCopy();
			std::set<const Terrain*> visible_chunk_set;
			std::set<const Terrain*> forbidden_chunks;

			auto exclude_neighborhood = [&](const Terrain* chunk) {
				forbidden_chunks.insert(chunk);
				auto neighbors = get_neighbors(chunk, visible_chunks);
				for (const auto& neighbor : neighbors) {
					forbidden_chunks.insert(neighbor);
					// Also exclude neighbors of neighbors for a wider "one per hilltop" effect
					auto n2 = get_neighbors(neighbor, visible_chunks);
					forbidden_chunks.insert(n2.begin(), n2.end());
				}
			};

			for (const auto& pair : spawned_launchers_) {
				exclude_neighborhood(pair.first);
			}

			struct SpawnCandidate {
				const Terrain* chunk;
				glm::vec3      point;
				float          height;
			};

			std::vector<SpawnCandidate> candidates;
			std::set<const Terrain*>    processed_chunks;

			for (const auto& chunk_ptr : visible_chunks) {
				const Terrain* chunk = chunk_ptr.get();
				visible_chunk_set.insert(chunk);
				if (processed_chunks.count(chunk)) {
					continue;
				}

				auto                        neighbors = get_neighbors(chunk, visible_chunks);
				std::vector<const Terrain*> current_grid = neighbors;
				current_grid.push_back(chunk);

				const Terrain* best_chunk = nullptr;
				glm::vec3      highest_point = {0, -std::numeric_limits<float>::infinity(), 0};

				for (const auto& grid_chunk : current_grid) {
					if (grid_chunk->proxy.highestPoint.y > highest_point.y) {
						highest_point = grid_chunk->proxy.highestPoint;
						best_chunk = grid_chunk;
					}
					processed_chunks.insert(grid_chunk);
				}

				if (best_chunk) {
					candidates.push_back({best_chunk, highest_point, highest_point.y});
				}
			}

			std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
				return a.height > b.height;
			});

			for (const auto& candidate : candidates) {
				if (forbidden_chunks.count(candidate.chunk) || launcher_cooldowns_.count(candidate.chunk)) {
					continue;
				}

				glm::vec3 chunk_pos = glm::vec3(
					candidate.chunk->GetX(),
					candidate.chunk->GetY(),
					candidate.chunk->GetZ()
				);
				glm::vec3 world_pos = chunk_pos + candidate.point;
				auto [terrain_h, terrain_normal] = vis->GetTerrainPointPropertiesThreadSafe(world_pos.x, world_pos.z);

				if (terrain_h >= 40) {
					glm::vec3 up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
					glm::quat terrain_alignment = glm::rotation(up_vector, terrain_normal);

					// Use a more robust ID based on chunk indices to avoid collisions and NaNs
					int ix = static_cast<int>(
						std::round(chunk_pos.x / static_cast<float>(Constants::Class::Terrain::ChunkSize()))
					);
					int iz = static_cast<int>(
						std::round(chunk_pos.z / static_cast<float>(Constants::Class::Terrain::ChunkSize()))
					);
					int id = 0x50000000 | ((ix + 1024) << 11) | (iz + 1024);

					QueueAddEntity<GuidedMissileLauncher>(
						id,
						Vector3(world_pos.x, terrain_h, world_pos.z),
						terrain_alignment
					);
					spawned_launchers_[candidate.chunk] = id;
					exclude_neighborhood(candidate.chunk);
				}
			}

			for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_launchers_.erase(it);
				} else if (GetEntity(it->second) == nullptr) {
					// Launcher was destroyed!
					launcher_cooldowns_[it->first] = time + 30.0f; // 30 second cooldown
					it = spawned_launchers_.erase(it);
				} else {
					++it;
				}
			}

			// Clean up expired cooldowns
			for (auto it = launcher_cooldowns_.begin(); it != launcher_cooldowns_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end() || time >= it->second) {
					it = launcher_cooldowns_.erase(it);
				} else {
					++it;
				}
			}
		}

		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = std::static_pointer_cast<PaperPlane>(targets[0]);
		if (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
			vis->UpdateHudGauge(3, {3, plane->GetHealth(), "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

			auto new_time = damage_dist_(eng_);

			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", true);
				// vis->TogglePostProcessingEffect("Time Stutter", true);
			}

			damage_timer_ = damage_timer_ + new_time;
		}

		damage_timer_ = std::min(damage_timer_, 2.0f);
	}

} // namespace Boidsish
