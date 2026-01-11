#include "PaperPlaneHandler.h"

#include <algorithm>
#include <set>

#include "GuidedMissileLauncher.h"
#include "PaperPlane.h"
#include "TracerRound.h"
#include "GuidedMissile.h"
#include "graphics.h"
#include "hud.h"
#include "neighbor_utils.h"
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Define the global weapon selection variable here
	int selected_weapon = 0;

	PaperPlaneHandler::PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool):
		SpatialEntityHandler(thread_pool), eng_(rd_()) {}

	void PaperPlaneHandler::PreTimestep(float time, float delta_time) {
		if (damage_timer_ > 0.0f) {
			damage_timer_ -= delta_time;
			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", false);
				// vis->TogglePostProcessingEffect("Time Stutter", false);
			}
		}

		if (vis && vis->GetTerrainGenerator()) {
			const auto&              visible_chunks = vis->GetTerrainGenerator()->getVisibleChunks();
			std::set<const Terrain*> visible_chunk_set;
			std::set<const Terrain*> forbidden_chunks;

			for (const auto& pair : spawned_launchers_) {
				const Terrain* existing_chunk = pair.first;
				auto           neighbors = get_neighbors(existing_chunk, visible_chunks);
				forbidden_chunks.insert(neighbors.begin(), neighbors.end());
				forbidden_chunks.insert(existing_chunk);
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
				if (forbidden_chunks.count(candidate.chunk)) {
					continue;
				}

				glm::vec3 chunk_pos = glm::vec3(
					candidate.chunk->GetX(),
					candidate.chunk->GetY(),
					candidate.chunk->GetZ()
				);
				glm::vec3 world_pos = chunk_pos + candidate.point;
				auto [terrain_h, terrain_normal] = vis->GetTerrainPointProperties(world_pos.x, world_pos.z);

				if (terrain_h >= 40) {
					glm::quat base_rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
					glm::vec3 up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
					glm::quat terrain_alignment = glm::rotation(up_vector, terrain_normal);
					glm::quat final_orientation = terrain_alignment * base_rotation;

					int id = chunk_pos.x + 10 * chunk_pos.y + 100 * chunk_pos.z;
					QueueAddEntity<GuidedMissileLauncher>(
						id,
						Vector3(world_pos.x, world_pos.y, world_pos.z),
						final_orientation
					);
					spawned_launchers_[candidate.chunk] = id;

					auto neighbors = get_neighbors(candidate.chunk, visible_chunks);
					forbidden_chunks.insert(neighbors.begin(), neighbors.end());
					forbidden_chunks.insert(candidate.chunk);
				}
			}

			for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end();) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_launchers_.erase(it);
				} else {
					++it;
				}
			}
		}

		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = targets[0];
		if (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
            vis->UpdateHudGauge(3, HudGauge{3, plane->GetHealth(), "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

			auto new_time = damage_dist_(eng_);

			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", true);
				// vis->TogglePostProcessingEffect("Time Stutter", true);
			}

			damage_timer_ = damage_timer_ + new_time;
		}

		damage_timer_ = std::min(damage_timer_, 5.0f);
	}

    void PaperPlaneHandler::AddEntity(int id, std::shared_ptr<EntityBase> entity) {
        SpatialEntityHandler::AddEntity(id, entity);
        if (auto launcher = std::dynamic_pointer_cast<GuidedMissileLauncher>(entity)) {
            launcher->Initialize();
        }
    }

    void PaperPlaneHandler::PostTimestep(float time, float delta_time) {
        SpatialEntityHandler::PostTimestep(time, delta_time);
        // Collision detection between tracer rounds and missiles
        auto tracers = GetEntitiesByType<TracerRound>();
        auto missiles = GetEntitiesByType<GuidedMissile>();

        for (const auto& tracer : tracers) {
            for (const auto& missile : missiles) {
                if ((tracer->GetPosition() - missile->GetPosition()).MagnitudeSquared() < 10.0f * 10.0f) {
                    // Collision detected
                    QueueRemoveEntity(tracer->GetId());
                    QueueRemoveEntity(missile->GetId());

                    // Create explosion
                    vis->AddFireEffect(
                        missile->GetPosition().Toglm(),
                        FireEffectStyle::Explosion,
                        glm::vec3(0.0f, 1.0f, 0.0f),
                        glm::vec3(0.0f),
                        1000,
                        2.0f
                    );
                }
            }
        }
    }

    std::vector<std::shared_ptr<Shape>> PaperPlaneHandler::operator()(float time) {
        // First, run the base class operator to get the initial list of shapes
        auto shapes = EntityHandler::operator()(time);

        // Add lasers from paper plane and missile launchers
        auto planes = GetEntitiesByType<PaperPlane>();
        if (!planes.empty()) {
            shapes.push_back(planes[0]->GetCannon()->GetLaser());
        }

        auto launchers = GetEntitiesByType<GuidedMissileLauncher>();
        for (const auto& launcher : launchers) {
            shapes.push_back(launcher->GetCannon()->GetLaser());
        }

        return shapes;
    }

} // namespace Boidsish
