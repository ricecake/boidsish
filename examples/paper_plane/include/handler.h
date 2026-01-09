#pragma once

// #include <iostream>
// #include <memory>
// #include <random>
// #include <set>
// #include <vector>

// #include "arrow.h"
// #include "bomb.h"
// #include "dot.h"
#include "emplacements.h"
// #include "field.h"
// #include "graphics.h"
#include "guided_missile.h"
#include "hud.h"
// #include "logger.h"
// #include "model.h"
#include "plane.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
// #include <GLFW/glfw3.h>
// #include <fire_effect.h>
// #include <glm/gtc/constants.hpp>
// #include <glm/gtc/quaternion.hpp>
// #include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

std::vector<const Terrain*>
get_neighbors(const Terrain* chunk, const std::vector<std::shared_ptr<Terrain>>& all_chunks) {
	std::vector<const Terrain*> neighbors;
	int                         center_x = chunk->GetX();
	int                         center_z = chunk->GetZ();
	float                       chunk_size = std::sqrt(chunk->proxy.radiusSq) * 2.0f;

	for (const auto& other_chunk_ptr : all_chunks) {
		const Terrain* other_chunk = other_chunk_ptr.get();
		if (other_chunk == chunk) {
			continue;
		}

		int other_x = other_chunk->GetX();
		int other_z = other_chunk->GetZ();

		bool is_neighbor = std::abs(other_x - center_x) <= chunk_size && std::abs(other_z - center_z) <= chunk_size;

		if (is_neighbor) {
			neighbors.push_back(other_chunk);
		}
	}

	return neighbors;
}

class GuidedMissileLauncher;

class PaperPlaneHandler: public SpatialEntityHandler {
public:
	PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool):
		SpatialEntityHandler(thread_pool), eng_(rd_()) {}

	void PreTimestep(float time, float delta_time) {
		if (damage_timer_ > 0.0f) {
			damage_timer_ -= delta_time;
			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", false);
				vis->TogglePostProcessingEffect("Time Stutter", false);
			}
		}

		// --- Guided Missile Launcher Spawning/Despawning ---
		if (vis && vis->GetTerrainGenerator()) {
			const auto&              visible_chunks = vis->GetTerrainGenerator()->getVisibleChunks();
			std::set<const Terrain*> visible_chunk_set;
			// --- Pre-populate forbidden zones from existing launchers ---
			std::set<const Terrain*> forbidden_chunks;
			for (const auto& pair : spawned_launchers_) {
				const Terrain* existing_chunk = pair.first;
				auto           neighbors = get_neighbors(existing_chunk, visible_chunks);
				forbidden_chunks.insert(neighbors.begin(), neighbors.end());
				forbidden_chunks.insert(existing_chunk);
			}

			// --- Pass 1: Candidate Gathering ---
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

			// --- Pass 2: Greedy Placement ---
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
					// Spawn it
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

					// Forbid spawning in neighbors for the rest of this frame
					auto neighbors = get_neighbors(candidate.chunk, visible_chunks);
					forbidden_chunks.insert(neighbors.begin(), neighbors.end());
					forbidden_chunks.insert(candidate.chunk);
				}
			}

			// Despawn old launchers
			for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end(); /* no increment */) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_launchers_.erase(it);
				} else {
					++it;
				}
			}
		}

		// --- Missile Spawning Logic ---
		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = std::static_pointer_cast<PaperPlane>(targets[0]);
		if (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
			vis->UpdateHudGauge(3, {3, plane->GetHealth(), "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

			auto new_time = damage_dist_(eng_);

			if (damage_timer_ <= 0.0f) { // Only trigger if not already active
				vis->TogglePostProcessingEffect("Glitch", true);
				vis->TogglePostProcessingEffect("Time Stutter", true);
			}

			damage_timer_ = damage_timer_ + new_time;
		}

		damage_timer_ = std::min(damage_timer_, 5.0f);

		/*
		        auto  ppos = plane->GetPosition();
		        float max_h = vis->GetTerrainMaxHeight();

		        float start_h = 0.0f;
		        float extreme_h = 0.0f;

		        // If terrain is not loaded, use a fallback height.
		        if (max_h <= 0.0f) {
		            start_h = 50.0f; // Start firing when plane is reasonably high
		            extreme_h = 200.0f;
		        } else {
		            start_h = (2.0f / 3.0f) * max_h;
		            extreme_h = 3.0f * max_h;
		        }

		        if (ppos.y < start_h)
		            return;
		        const float p_min = 0.5f;  // Missiles per second at start_h
		        const float p_max = 10.0f; // Missiles per second at extreme_h

		        float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
		        norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f); // clamp

		        float missiles_per_second = p_min * pow((p_max / p_min), norm_alt);
		        float fire_probability_this_frame = missiles_per_second * delta_time;

		        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		        if (dist(eng_) < fire_probability_this_frame) {
		            // --- Calculate Firing Location ---
		            // We want to fire from a "rainbow" arc on the terrain that is visible to the camera.

		            // 1. Get camera properties
		            const Camera& camera = vis->GetCamera();
		            glm::vec3     cam_pos = glm::vec3(camera.x, camera.y, camera.z);

		            // This calculation ensures we get the camera's actual forward direction,
		            // even in chase cam mode.
		            glm::vec3 plane_pos_glm = glm::vec3(ppos.x, ppos.y, ppos.z);
		            glm::vec3 cam_fwd = glm::normalize(plane_pos_glm - cam_pos);
		            glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, glm::vec3(0.0f, 1.0f, 0.0f)));

		            // 2. Define spawn arc parameters
		            const float kMinSpawnDist = 250.0f;
		            const float kMaxSpawnDist = 400.0f;
		            const float kSpawnFov = glm::radians(camera.fov * 0.9f); // Just under camera FOV

		            // 3. Generate random point in the arc
		            std::uniform_real_distribution<float> dist_dist(kMinSpawnDist, kMaxSpawnDist);
		            std::uniform_real_distribution<float> dist_angle(-kSpawnFov / 2.0f, kSpawnFov / 2.0f);

		            float     rand_dist = dist_dist(eng_);
		            float     rand_angle = dist_angle(eng_);
		            glm::vec3 rand_dir = glm::angleAxis(rand_angle, glm::vec3(0.0f, 1.0f, 0.0f)) * cam_fwd;

		            // 4. Find the point on the terrain
		            glm::vec3 ray_origin = cam_pos;
		            // We push the origin forward a bit to ensure the spawn is always in front and far away
		            ray_origin += rand_dir * rand_dist;

		            float terrain_h = 0.0f;
		            if (max_h > 0.0f) {
		                std::tuple<float, glm::vec3> props = vis->GetTerrainPointProperties(ray_origin.x, ray_origin.z);
		                terrain_h = std::get<0>(props);

		                // Safety check: ensure missile doesn't spawn underground or too high if terrain is weird
		                if (terrain_h < 0.0f || !std::isfinite(terrain_h)) {
		                    return;
		                }
		            }

		            Vector3 launchPos = Vector3(ray_origin.x, terrain_h, ray_origin.z);

		            QueueAddEntity<GuidedMissile>(launchPos);
		        }
		*/
	}

private:
	std::map<const Terrain*, int>         spawned_launchers_;
	std::random_device                    rd_;
	std::mt19937                          eng_;
	float                                 damage_timer_ = 0.0f;
	std::uniform_real_distribution<float> damage_dist_;
};
