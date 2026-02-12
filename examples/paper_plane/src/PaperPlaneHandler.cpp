#include "PaperPlaneHandler.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "GuidedMissileLauncher.h"
#include "PearEnemy.h"
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

	void PaperPlaneHandler::AddScore(int delta, const std::string& label) const {
		if (score_indicator_)
			score_indicator_->AddScore(delta, label);
	}

	void PaperPlaneHandler::PreparePlane(std::shared_ptr<PaperPlane> plane) {
		if (!plane || !vis)
			return;

		float     best_score = -1e10f;
		glm::vec3 best_pos(0, 100, 0);
		glm::vec3 best_dir(0, 0, -1);

		// Sample a grid around the starting area
		for (float x = -300.0f; x <= 300.0f; x += 30.0f) {
			for (float z = -300.0f; z <= 300.0f; z += 30.0f) {
				auto [h, norm] = vis->GetTerrain()->GetPointProperties(x, z);
				if (h < 15.0f)
					continue; // Avoid valleys/water

				// Test 8 directions
				for (int i = 0; i < 8; ++i) {
					float     angle = i * (glm::pi<float>() / 4.0f);
					glm::vec3 dir(sin(angle), 0, cos(angle));

					// Look ahead to check gradient
					float check_dist = 60.0f;
					auto [h_ahead, norm_ahead] = vis->GetTerrain()->GetPointProperties(
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

		if (vis && vis->GetTerrain()) {
			const auto                    visible_chunks = vis->GetTerrain()->GetVisibleChunksCopy();
			std::set<std::pair<int, int>> visible_chunk_set;
			for (const auto& chunk_ptr : visible_chunks) {
				visible_chunk_set.insert({static_cast<int>(chunk_ptr->GetX()), static_cast<int>(chunk_ptr->GetZ())});
			}

			// 1. Detect removals and destructions BEFORE spawning new ones
			for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_launchers_.erase(it);
				} else if (GetEntity(it->second) == nullptr) {
					// Launcher was destroyed! (Points are awarded in GuidedMissileLauncher::Destroy)
					launcher_cooldowns_[it->first] = time + 30.0f; // 30 second cooldown
					it = spawned_launchers_.erase(it);
				} else {
					++it;
				}
			}

			for (auto it = spawned_roamers_.begin(); it != spawned_roamers_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_roamers_.erase(it);
				} else if (GetEntity(it->second) == nullptr) {
					// Roamer was destroyed or despawned
					it = spawned_roamers_.erase(it);
				} else {
					++it;
				}
			}

			// 2. Clean up expired cooldowns
			for (auto it = launcher_cooldowns_.begin(); it != launcher_cooldowns_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end() || time >= it->second) {
					it = launcher_cooldowns_.erase(it);
				} else {
					++it;
				}
			}

			// 3. Populate forbidden coordinates based on CURRENT launchers and cooldowns
			std::set<std::pair<int, int>> forbidden_coords;
			auto                          exclude_neighborhood = [&](const std::pair<int, int>& coord) {
                int cx = coord.first;
                int cz = coord.second;
                int step = Constants::Class::Terrain::ChunkSize();
                int range = 2; // Exclude 2 chunks in every direction
                for (int dx = -range; dx <= range; ++dx) {
                    for (int dz = -range; dz <= range; ++dz) {
                        forbidden_coords.insert({cx + dx * step, cz + dz * step});
                    }
                }
			};

			for (const auto& pair : spawned_launchers_) {
				exclude_neighborhood(pair.first);
			}

			for (const auto& pair : spawned_roamers_) {
				exclude_neighborhood(pair.first);
			}

			for (const auto& pair : launcher_cooldowns_) {
				exclude_neighborhood(pair.first);
			}

			struct SpawnCandidate {
				const Terrain* chunk;
				glm::vec3      point;
				float          height;
			};

			std::vector<SpawnCandidate> launcher_candidates;
			std::vector<SpawnCandidate> roamer_candidates;
			std::set<const Terrain*>    processed_chunks;

			for (const auto& chunk_ptr : visible_chunks) {
				const Terrain* chunk = chunk_ptr.get();
				visible_chunk_set.insert({static_cast<int>(chunk->GetX()), static_cast<int>(chunk->GetZ())});
				if (processed_chunks.count(chunk)) {
					continue;
				}

				auto                        neighbors = get_neighbors(chunk, visible_chunks);
				std::vector<const Terrain*> current_grid = neighbors;
				current_grid.push_back(chunk);

				const Terrain* best_launcher_chunk = nullptr;
				glm::vec3      highest_point = {0, -std::numeric_limits<float>::infinity(), 0};

				const Terrain* best_roamer_chunk = nullptr;
				glm::vec3      lowest_flat_point = {0, std::numeric_limits<float>::infinity(), 0};

				for (const auto& grid_chunk : current_grid) {
					if (grid_chunk->proxy.highestPoint.y > highest_point.y) {
						highest_point = grid_chunk->proxy.highestPoint;
						best_launcher_chunk = grid_chunk;
					}

					// Find a low flat point for roamer
					// For simplicity, we can use a sample or a property if available.
					// Terrain doesn't have lowestPoint in proxy. Let's use a sample.
					// Actually, let's just use the chunk center for roamers if it's low enough.
					float cx = static_cast<float>(grid_chunk->GetX()) + Constants::Class::Terrain::ChunkSize() * 0.5f;
					float cz = static_cast<float>(grid_chunk->GetZ()) + Constants::Class::Terrain::ChunkSize() * 0.5f;
					auto [h, norm] = vis->GetTerrain()->GetPointProperties(cx, cz);
					if (h < lowest_flat_point.y && norm.y > 0.9f) {
						lowest_flat_point = glm::vec3(cx - grid_chunk->GetX(), h, cz - grid_chunk->GetZ());
						best_roamer_chunk = grid_chunk;
					}

					processed_chunks.insert(grid_chunk);
				}

				if (best_launcher_chunk) {
					launcher_candidates.push_back({best_launcher_chunk, highest_point, highest_point.y});
				}
				if (best_roamer_chunk && lowest_flat_point.y < 30.0f) {
					roamer_candidates.push_back({best_roamer_chunk, lowest_flat_point, lowest_flat_point.y});
				}
			}

			std::sort(launcher_candidates.begin(), launcher_candidates.end(), [](const auto& a, const auto& b) {
				return a.height > b.height;
			});

			for (const auto& candidate : launcher_candidates) {
				if (forbidden_coords.count(
						{static_cast<int>(candidate.chunk->GetX()), static_cast<int>(candidate.chunk->GetZ())}
					)) {
					continue;
				}

				glm::vec3 chunk_pos = glm::vec3(
					candidate.chunk->GetX(),
					candidate.chunk->GetY(),
					candidate.chunk->GetZ()
				);
				glm::vec3 world_pos = chunk_pos + candidate.point;
				auto [terrain_h, terrain_normal] = vis->GetTerrain()->GetPointProperties(world_pos.x, world_pos.z);

				if (terrain_h >= 40.0f) {
					glm::vec3 up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
					glm::quat terrain_alignment = glm::rotation(up_vector, terrain_normal);

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
					std::pair<int, int> coord = {
						static_cast<int>(candidate.chunk->GetX()),
						static_cast<int>(candidate.chunk->GetZ())
					};
					spawned_launchers_[coord] = id;
					exclude_neighborhood(coord);
				}
			}

			// Spawn Roamers
			auto planes = GetEntitiesByType<PaperPlane>();
			glm::vec3 plane_pos(0.0f);
			bool has_plane = !planes.empty();
			if (has_plane) {
				plane_pos = planes[0]->GetPosition().Toglm();
			}

			for (const auto& candidate : roamer_candidates) {
				if (forbidden_coords.count(
						{static_cast<int>(candidate.chunk->GetX()), static_cast<int>(candidate.chunk->GetZ())}
					)) {
					continue;
				}

				glm::vec3 chunk_pos = glm::vec3(
					candidate.chunk->GetX(),
					candidate.chunk->GetY(),
					candidate.chunk->GetZ()
				);
				glm::vec3 world_pos = chunk_pos + candidate.point;
				auto [terrain_h, terrain_normal] = vis->GetTerrain()->GetPointProperties(world_pos.x, world_pos.z);

				if (terrain_h < 35.0f) {
					// Prevent spawn/despawn loop: only spawn if player is within 1000m
					if (has_plane && glm::distance(world_pos, plane_pos) > 1000.0f) {
						continue;
					}
					int ix = static_cast<int>(
						std::round(chunk_pos.x / static_cast<float>(Constants::Class::Terrain::ChunkSize()))
					);
					int iz = static_cast<int>(
						std::round(chunk_pos.z / static_cast<float>(Constants::Class::Terrain::ChunkSize()))
					);
					int id = 0x60000000 | ((ix + 1024) << 11) | (iz + 1024);

					QueueAddEntity<PearEnemy>(
						id,
						Vector3(static_cast<float>(world_pos.x), static_cast<float>(terrain_h + 0.1f), static_cast<float>(world_pos.z))
					);
					std::pair<int, int> coord = {
						static_cast<int>(candidate.chunk->GetX()),
						static_cast<int>(candidate.chunk->GetZ())
					};
					spawned_roamers_[coord] = id;
					exclude_neighborhood(coord);
				}
			}
		}

		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = std::static_pointer_cast<PaperPlane>(targets[0]);
		bool took_damage = false;
		while (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
			took_damage = true;

			auto new_time = damage_dist_(eng_);

			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", true);
				// vis->TogglePostProcessingEffect("Time Stutter", true);
			}

			damage_timer_ = damage_timer_ + new_time;
		}

		if (took_damage && health_gauge_) {
			health_gauge_->SetValue(plane->GetHealth() / plane->GetMaxHealth());
		}

		damage_timer_ = std::min(damage_timer_, 2.0f);
	}

} // namespace Boidsish
