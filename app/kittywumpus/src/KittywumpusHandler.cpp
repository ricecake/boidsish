#include "KittywumpusHandler.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "CongaMarcher.h"
#include "GuidedMissileLauncher.h"
#include "KittywumpusPlane.h"
#include "Potshot.h"
#include "Swooper.h"
#include "VortexFlockingEntity.h"
#include "checkpoint_ring.h"
#include "constants.h"
#include "graphics.h"
#include "hud.h"
#include "neighbor_utils.h"
#include "terrain_generator.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

// Define the global weapon selection variable
int kittywumpus_selected_weapon = 0;

KittywumpusHandler::KittywumpusHandler(task_thread_pool::task_thread_pool& thread_pool):
	SpatialEntityHandler(thread_pool), eng_(rd_()), damage_dist_(0.25f, 0.5f) {
}

void KittywumpusHandler::RecordTarget(std::shared_ptr<EntityBase> target) const {
	if (target) {
		std::lock_guard<std::mutex> lock(target_mutex_);
		target_counts_[target->GetId()]++;
	}
}

int KittywumpusHandler::GetTargetCount(std::shared_ptr<EntityBase> target) const {
	if (target) {
		std::lock_guard<std::mutex> lock(target_mutex_);
		auto                        it = target_counts_.find(target->GetId());
		if (it != target_counts_.end()) {
			return it->second;
		}
	}
	return 0;
}

int KittywumpusHandler::GetScore() const {
	if (score_indicator_)
		return score_indicator_->GetValue();
	return 0;
}

void KittywumpusHandler::AddScore(int delta, const std::string& label) const {
	if (score_indicator_)
		score_indicator_->AddScore(delta, label);
}

void KittywumpusHandler::OnPlaneDeath(int score) const {
	if (vis) {
		game_over_msg_ = vis->AddHudMessage("GAME OVER", HudAlignment::MIDDLE_CENTER, {0, -30}, 3.0f);
		score_msg_ = vis->AddHudMessage("Final Score: " + std::to_string(score), HudAlignment::MIDDLE_CENTER, {0, 30}, 1.5f);
		restart_msg_ = vis->AddHudMessage("Press any key to return to menu", HudAlignment::MIDDLE_CENTER, {0, 80}, 1.0f);
	}
}

void KittywumpusHandler::ClearGameOverHUD() {
	if (game_over_msg_) {
		game_over_msg_->SetVisible(false);
		game_over_msg_.reset();
	}
	if (score_msg_) {
		score_msg_->SetVisible(false);
		score_msg_.reset();
	}
	if (restart_msg_) {
		restart_msg_->SetVisible(false);
		restart_msg_.reset();
	}
}

void KittywumpusHandler::PreparePlane(std::shared_ptr<KittywumpusPlane> plane) {
	if (!plane || !vis || !vis->GetTerrain())
		return;

	glm::vec3 best_pos(210, 30, -600);
	glm::vec3 best_dir(0, 0, -1);

	plane->SetPosition(best_pos.x, best_pos.y, best_pos.z);
	glm::quat orient = glm::quatLookAt(glm::normalize(best_dir), glm::vec3(0, 1, 0));
	plane->SetOrientation(orient);
	plane->SetVelocity(best_dir * 60.0f);
	plane->UpdateShape();

	// Update camera to follow
	vis->GetCamera().x = best_pos.x;
	vis->GetCamera().y = best_pos.y + 5;
	vis->GetCamera().z = best_pos.z + 10;
}

void KittywumpusHandler::ShowFlightHUD() {
	// Health gauge and other flight HUD elements are managed by main
	// This is called when transitioning back to flight mode
	if (health_gauge_) {
		// The gauge should be visible in flight mode
	}
}

void KittywumpusHandler::HideFlightHUD() {
	// Hide flight-specific HUD elements when in FPS mode
}

void KittywumpusHandler::ShowFPSHUD() {
	// Show FPS-specific HUD (crosshair added in main)
}

void KittywumpusHandler::HideFPSHUD() {
	// Hide FPS-specific HUD when transitioning back to flight
}

void KittywumpusHandler::RemoveEntity(int id) {
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
			auto targets = GetEntitiesByType<KittywumpusPlane>();
			if (!targets.empty()) {
				targets[0]->AddHealth(10.0f);
				if (health_gauge_) {
					health_gauge_->SetValue(targets[0]->GetHealth() / targets[0]->GetMaxHealth());
				}
			}
			break;
		}
		case CheckpointStatus::MISSED:
			streak_ = 0;
			last_collected_sequence_id_ = ring->GetSequenceId();
			break;
		case CheckpointStatus::EXPIRED:
		case CheckpointStatus::OUT_OF_RANGE:
			if (ring->GetSequenceId() > last_collected_sequence_id_) {
				streak_ = 0;
			}
			break;
		case CheckpointStatus::PRUNED:
			last_collected_sequence_id_ = std::max(last_collected_sequence_id_, ring->GetSequenceId());
			break;
		default:
			break;
		}

		if (streak_indicator_) {
			streak_indicator_->SetValue(static_cast<float>(streak_));
		}
	}
	SpatialEntityHandler::RemoveEntity(id);
}

void KittywumpusHandler::PreTimestep(float time, float delta_time) {
	{
		std::lock_guard<std::mutex> lock(target_mutex_);
		target_counts_.clear();
	}

	// Skip all enemy logic when in main menu
	if (in_main_menu_) {
		return;
	}

	// Skip enemy spawning and launcher management if not flying
	// INTEGRATION_POINT: Add ground enemy spawning when is_flying_ == false
	if (!is_flying_) {
		// In FPS mode, we might want different enemy behavior
		// For now, just skip aerial threats
		return;
	}

	if (damage_timer_ > 0.0f) {
		damage_timer_ -= delta_time;
		if (damage_timer_ <= 0.0f) {
			vis->TogglePostProcessingEffect("Glitch", false);
		}
	}

	if (vis && vis->GetTerrain()) {
		const auto                    visible_chunks = vis->GetTerrain()->GetVisibleChunksCopy();
		std::set<std::pair<int, int>> visible_chunk_set;
		for (const auto& chunk_ptr : visible_chunks) {
			visible_chunk_set.insert({static_cast<int>(chunk_ptr->GetX()), static_cast<int>(chunk_ptr->GetZ())});
		}

		// Detect removals and destructions
		for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end();) {
			if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
				QueueRemoveEntity(it->second);
				it = spawned_launchers_.erase(it);
			} else if (GetEntity(it->second) == nullptr) {
				launcher_cooldowns_[it->first] = time + 30.0f;
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

		// Populate forbidden coordinates
		std::set<std::pair<int, int>> forbidden_coords;
		auto                          exclude_neighborhood = [&](const std::pair<int, int>& coord) {
			int cx = coord.first;
			int cz = coord.second;
			int step = Constants::Class::Terrain::ChunkSize();
			int range = 3;
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

	auto targets = GetEntitiesByType<KittywumpusPlane>();
	if (targets.empty())
		return;

	auto plane = std::static_pointer_cast<KittywumpusPlane>(targets[0]);
	bool took_damage = false;
	while (plane && plane->IsDamagePending()) {
		plane->AcknowledgeDamage();
		took_damage = true;

		auto new_time = damage_dist_(eng_);

		if (damage_timer_ <= 0.0f) {
			vis->TogglePostProcessingEffect("Glitch", true);
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
KittywumpusHandler::FindOccludedSpawnPosition(const glm::vec3& player_pos, const glm::vec3& player_forward) {
	std::uniform_real_distribution<float> dist_range(300.0f, 600.0f);
	std::uniform_real_distribution<float> angle_range(-0.5f, 0.5f);

	glm::vec3 up(0, 1, 0);
	glm::vec3 right = glm::normalize(glm::cross(player_forward, up));
	if (glm::length(right) < 0.001f)
		right = glm::vec3(1, 0, 0);

	glm::vec3 camera_pos = vis->GetCamera().pos();

	for (int i = 0; i < 15; ++i) {
		float d = dist_range(eng_);
		float a = angle_range(eng_);

		glm::vec3 candidate = player_pos + player_forward * d + right * (a * d);
		auto [h, norm] = GetTerrainPropertiesAtPoint(candidate.x, candidate.z);
		(void)norm;
		candidate.y = h + 40.0f;

		glm::vec3 to_candidate = candidate - camera_pos;
		float     dist_to_cand = glm::length(to_candidate);
		glm::vec3 dir = glm::normalize(to_candidate);

		float     hit_dist;
		glm::vec3 hit_norm;
		if (RaycastTerrain(camera_pos, dir, dist_to_cand, hit_dist, hit_norm)) {
			glm::vec3 hit_point = camera_pos + dir * hit_dist;
			if (glm::dot(player_forward, hit_point - player_pos) > 0) {
				return candidate;
			}
		}
	}
	return std::nullopt;
}

} // namespace Boidsish
