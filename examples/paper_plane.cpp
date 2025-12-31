#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "graphics.h"
#include "logger.h"
#include "paper_plane.h"
#include "guided_missile.h"
#include "missile_launcher.h"
#include "spatial_entity_handler.h"
#include "terrain.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <optional>

using namespace Boidsish;

class PaperPlaneHandler : public SpatialEntityHandler {
public:
    PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool)
        : SpatialEntityHandler(thread_pool), eng_(rd_()), time_since_last_spawn_check_(0.0f) {}

    void PreTimestep(float time, float delta_time) {
        time_since_last_spawn_check_ += delta_time;

        auto targets = GetEntitiesByType<PaperPlane>();
        if (targets.empty())
            return;

        auto plane = targets[0];

        // --- Manage MissileLauncher Lifecycle ---
        DespawnLaunchers(plane);

        // Only check for new spawn locations periodically to save performance.
        if (time_since_last_spawn_check_ > 1.0f) {
            SpawnLaunchers(plane);
            time_since_last_spawn_check_ = 0.0f;
        }
    }

private:
    void DespawnLaunchers(const std::shared_ptr<PaperPlane>& plane) {
        const float kDespawnDistance = 500.0f;
        auto launchers = GetEntitiesByType<MissileLauncher>();
        for (const auto& launcher : launchers) {
            if ((launcher->GetPosition() - plane->GetPosition()).Magnitude() > kDespawnDistance) {
                QueueRemoveEntity(launcher->GetId());
            }
        }
    }

    void SpawnLaunchers(const std::shared_ptr<PaperPlane>& plane) {
        if (!vis) return;

        auto chunks = vis->GetTerrainChunks();
        auto existing_launchers = GetEntitiesByType<MissileLauncher>();

        std::set<std::pair<int, int>> occupied_chunks;
        for (const auto& launcher : existing_launchers) {
            auto pos = launcher->GetPosition();
            occupied_chunks.insert({
                static_cast<int>(std::floor(pos.x / 32.0f)),
                static_cast<int>(std::floor(pos.z / 32.0f))
            });
        }

        for (const auto& chunk : chunks) {
            if (!chunk) continue;

            auto chunk_coords = std::make_pair(
                static_cast<int>(std::floor(chunk->proxy.center.x / 32.0f)),
                static_cast<int>(std::floor(chunk->proxy.center.z / 32.0f))
            );

            if (occupied_chunks.count(chunk_coords)) continue;

            const float kMinHeight = 60.0f;
            if (chunk->proxy.maxY < kMinHeight) continue;

            // Check cache
            auto it = chunk_spawn_cache_.find(chunk_coords);
            if (it != chunk_spawn_cache_.end()) {
                if (it->second.has_value()) {
                    AddEntity<MissileLauncher>(*it->second);
                    break;
                }
                continue; // Cached failure
            }

            // If not in cache, calculate
            const int kGridSize = 5;
            const float kChunkStep = 32.0f / kGridSize;
            const float kMaxFlatnessDot = 0.98f;

            std::vector<glm::vec3> candidates;
            for (int x = 0; x < kGridSize; ++x) {
                for (int z = 0; z < kGridSize; ++z) {
                    float world_x = chunk->proxy.center.x - 16.0f + (x * kChunkStep);
                    float world_z = chunk->proxy.center.z - 16.0f + (z * kChunkStep);

                    auto [height, normal] = vis->GetTerrainPointProperties(world_x, world_z);

                    if (height >= kMinHeight && glm::dot(normal, {0, 1, 0}) > kMaxFlatnessDot) {
                        candidates.push_back({world_x, height, world_z});
                    }
                }
            }

            if (!candidates.empty()) {
                std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
                const auto& spawn_point = candidates[dist(eng_)];
                chunk_spawn_cache_[chunk_coords] = spawn_point;
                AddEntity<MissileLauncher>(spawn_point);
                break;
            } else {
                chunk_spawn_cache_[chunk_coords] = std::nullopt;
            }
        }
    }

    std::random_device rd_;
    std::mt19937 eng_;
    float time_since_last_spawn_check_;
    std::map<std::pair<int, int>, std::optional<glm::vec3>> chunk_spawn_cache_;
};

int main() {
    try {
        auto visualizer = std::make_shared<Visualizer>(1280, 720, "Paper Plane");

        Boidsish::Camera camera;
        visualizer->SetCamera(camera);
        auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

        auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
        handler.SetVisualizer(visualizer);
        auto id = handler.AddEntity<PaperPlane>();
        auto plane = std::dynamic_pointer_cast<PaperPlane>(handler.GetEntity(id));
        plane->SetPosition(0, height + 10, 0);

        visualizer->AddShapeHandler(std::ref(handler));
        visualizer->SetChaseCamera(plane);

        auto controller = std::make_shared<PaperPlaneInputController>();
        plane->SetController(controller);

        visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
            controller->pitch_up = state.keys[GLFW_KEY_S];
            controller->pitch_down = state.keys[GLFW_KEY_W];
            controller->yaw_left = state.keys[GLFW_KEY_A];
            controller->yaw_right = state.keys[GLFW_KEY_D];
            controller->roll_left = state.keys[GLFW_KEY_Q];
            controller->roll_right = state.keys[GLFW_KEY_E];
            controller->boost = state.keys[GLFW_KEY_SPACE];
        });

        visualizer->Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
