#include "PaperPlaneHandler.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "CongaMarcher.h"
#include "GuidedMissileLauncher.h"
#include "checkpoint_ring.h"
#include "PaperPlane.h"
#include "Potshot.h"
#include "Swooper.h"
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

	void PaperPlaneHandler::RecordTarget(std::shared_ptr<EntityBase> target) const {
		if (target) {
			std::lock_guard<std::mutex> lock(target_mutex_);
			target_counts_[target->GetId()]++;
		}
	}

	int PaperPlaneHandler::GetTargetCount(std::shared_ptr<EntityBase> target) const {
		if (target) {
			std::lock_guard<std::mutex> lock(target_mutex_);
			auto                        it = target_counts_.find(target->GetId());
			if (it != target_counts_.end()) {
				return it->second;
			}
		}
		return 0;
	}

	int PaperPlaneHandler::GetScore() const {
		if (score_indicator_)
			return score_indicator_->GetValue();
		return 0;
	}

	void PaperPlaneHandler::AddScore(int delta, const std::string& label) const {
		if (score_indicator_)
			score_indicator_->AddScore(delta, label);
	}

	void PaperPlaneHandler::OnPlaneDeath(int score) const {
		if (vis) {
			vis->AddHudMessage("GAME OVER", HudAlignment::MIDDLE_CENTER, {0, 0}, 3.0f);
			vis->AddHudMessage("Final Score: " + std::to_string(score), HudAlignment::MIDDLE_CENTER, {0, 60}, 1.5f);
		}
	}

	void PaperPlaneHandler::PreparePlane(std::shared_ptr<PaperPlane> plane) {
		if (!plane || !vis || !vis->GetTerrain())
			return;

		float     best_score = -1e10f;
		glm::vec3 best_pos(210, 30, -600);
		glm::vec3 best_dir(0, 0, -1);

		// Sample a grid around the starting area
		// for (float x = -400; x <= 400; x += 25) {
		// 	for (float z = -400; z <= 400; z += 25) {
		// 		glm::vec3 path_data = vis->GetTerrain()->GetPathData(x, z);
		// 		float     dist_from_path = std::abs(path_data.x);

		// 		auto [h, norm] = vis->GetTerrainPropertiesAtPoint(x, z);
		// 		if (h < 5.0f)
		// 			continue; // Avoid water

		// 		float path_score = 1.0f - glm::smoothstep(0.0f, 1.5f, dist_from_path);

		// 		// Tangent to the path: perpendicular to gradient (path_data.y, path_data.z)
		// 		glm::vec2 tangent2D = glm::normalize(glm::vec2(path_data.z, -path_data.y));
		// 		glm::vec2 directions[2] = {tangent2D, -tangent2D};

		// 		for (const auto& t2d : directions) {
		// 			glm::vec3 dir(t2d.x, 0, t2d.y);

		// 			// Look ahead to check gradient
		// 			float check_dist = 60.0f;
		// 			auto [h_ahead, norm_ahead] = vis->GetTerrainPropertiesAtPoint(
		// 				x + dir.x * check_dist,
		// 				z + dir.z * check_dist
		// 			);

		// 			float gradient = h - h_ahead; // positive is downslope
		// 			float score = path_score * 200.0f;
		// 			score -= h * 0.2f;        // Prefer lower altitude (valleys)
		// 			score += gradient * 5.0f; // Prefer downslope

		// 			// Penalize if pointing directly into a steep uphill
		// 			if (gradient < -5.0f)
		// 				score -= 100.0f;

		// 			if (score > best_score) {
		// 				best_score = score;
		// 				best_pos = glm::vec3(x, h + 30.0f, z); // Lower fly height to stay below aggression threshold
		// 				best_dir = dir;
		// 			}
		// 		}
		// 	}
		// }

		plane->SetPosition(best_pos.x, best_pos.y, best_pos.z);
		glm::quat orient = glm::quatLookAt(glm::normalize(best_dir), glm::vec3(0, 1, 0));
		plane->SetOrientation(orient);
		plane->SetVelocity(best_dir * 60.0f); // Set a good starting speed
		plane->UpdateShape();

		// Update camera to follow
		vis->GetCamera().x = best_pos.x;
		vis->GetCamera().y = best_pos.y + 5;
		vis->GetCamera().z = best_pos.z + 10;
	}

	void PaperPlaneHandler::RemoveEntity(int id) {
		auto entity = GetEntity(id);
		if (auto ring = std::dynamic_pointer_cast<CheckpointRing>(entity)) {
			switch (ring->GetStatus()) {
			case CheckpointStatus::COLLECTED: {
				if (ring->GetSequenceId() == last_collected_sequence_id_ + 1) {
					streak_++;
				} else {
					streak_ = 1;
				}
				last_collected_sequence_id_ = ring->GetSequenceId();

				int bonus = 100 * streak_;
				AddScore(bonus, "Streak x" + std::to_string(streak_));

				// Heal player
				auto targets = GetEntitiesByType<PaperPlane>();
				if (!targets.empty()) {
					targets[0]->AddHealth(10.0f);
					if (health_gauge_) {
						health_gauge_->SetValue(targets[0]->GetHealth() / targets[0]->GetMaxHealth());
					}
				}
				break;
			}
			case CheckpointStatus::EXPIRED:
			case CheckpointStatus::OUT_OF_RANGE:
				if (ring->GetSequenceId() > last_collected_sequence_id_) {
					streak_ = 0;
				}
				break;
			case CheckpointStatus::PRUNED:
			default:
				// Do nothing, keep streak
				break;
			}

			if (streak_indicator_) {
				streak_indicator_->SetValue(static_cast<float>(streak_));
			}
		}
		SpatialEntityHandler::RemoveEntity(id);
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
                int range = 3; // Exclude 2 chunks in every direction
                for (int dx = -range; dx <= range; ++dx) {
                    for (int dz = -range; dz <= range; ++dz) {
                        forbidden_coords.insert({cx + dx * step, cz + dz * step});
                    }
                }
			};

			for (const auto& pair : spawned_launchers_) {
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

			std::vector<SpawnCandidate> candidates;
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
				auto [terrain_h, terrain_normal] = vis->GetTerrainPropertiesAtPoint(world_pos.x, world_pos.z);

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

					QueueAddEntityWithId<GuidedMissileLauncher>(
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

		// Enemy spawning logic
		enemy_spawn_timer_ -= delta_time;
		if (enemy_spawn_timer_ <= 0) {
			enemy_spawn_timer_ = 6.0f + std::uniform_real_distribution<float>(0, 4.0f)(eng_);

			if (!targets.empty()) {
				auto pos = plane->GetPosition().Toglm();
				auto forward = plane->GetOrientation() * glm::vec3(0, 0, -1);
				auto spawn_pos = FindOccludedSpawnPosition(pos, forward);

				if (spawn_pos) {
					std::uniform_int_distribution<int> enemy_type(0, 2);
					int                                type = enemy_type(eng_);

					if (type == 0) {
						// Conga Marcher group
						int count = std::uniform_int_distribution<int>(3, 9)(eng_);
						int last_id = -1;
						for (int i = 0; i < count; ++i) {
							// For the first one, last_id is -1.
							// For subsequent ones, it's the id of the previous one.
							int new_id = AddEntity<CongaMarcher>(
								Vector3(spawn_pos->x, spawn_pos->y, spawn_pos->z),
								last_id
							);
							last_id = new_id;
						}
					} else if (type == 1) {
						// Swooper
						QueueAddEntity<Swooper>(Vector3(spawn_pos->x, spawn_pos->y, spawn_pos->z));
					} else {
						// Potshot
						QueueAddEntity<Potshot>(Vector3(spawn_pos->x, spawn_pos->y, spawn_pos->z));
					}
				}
			}
		}
	}

	std::optional<glm::vec3>
	PaperPlaneHandler::FindOccludedSpawnPosition(const glm::vec3& player_pos, const glm::vec3& player_forward) {
		std::uniform_real_distribution<float> dist_range(500.0f, 800.0f);
		std::uniform_real_distribution<float> angle_range(-0.5f, 0.5f);

		glm::vec3 up(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(player_forward, up));
		if (glm::length(right) < 0.001f)
			right = glm::vec3(1, 0, 0);

		for (int i = 0; i < 15; ++i) { // Try 15 times
			float d = dist_range(eng_);
			float a = angle_range(eng_);

			glm::vec3 candidate = player_pos + player_forward * d + right * (a * d);
			auto [h, norm] = GetTerrainPropertiesAtPoint(candidate.x, candidate.z);
			candidate.y = h + 40.0f; // Above ground

			// Check LOS
			glm::vec3 to_candidate = candidate - player_pos;
			float     dist_to_cand = glm::length(to_candidate);
			glm::vec3 dir = glm::normalize(to_candidate);

			float     hit_dist;
			glm::vec3 hit_norm;
			if (RaycastTerrain(player_pos, dir, dist_to_cand, hit_dist, hit_norm)) {
				// Hit terrain before reaching candidate -> Occluded!
				return candidate;
			}
		}
		return std::nullopt;
	}

} // namespace Boidsish
