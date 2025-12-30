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

        // 1. Identify chunks that already have a launcher
        std::set<std::pair<int, int>> occupied_chunks;
        for (const auto& launcher : existing_launchers) {
            auto pos = launcher->GetPosition();
            occupied_chunks.insert({
                static_cast<int>(std::floor(pos.x / 32.0f)),
                static_cast<int>(std::floor(pos.z / 32.0f))
            });
        }

        // 2. Iterate through visible chunks and filter them
        for (const auto& chunk : chunks) {
            if (!chunk) continue;

            auto chunk_coords = std::make_pair(
                static_cast<int>(std::floor(chunk->proxy.center.x / 32.0f)),
                static_cast<int>(std::floor(chunk->proxy.center.z / 32.0f))
            );

            // Skip if chunk is already occupied
            if (occupied_chunks.count(chunk_coords)) {
                continue;
            }

            // Skip if the entire chunk is too low
            const float kMinHeight = 60.0f;
            if (chunk->proxy.maxY < kMinHeight) {
                continue;
            }

            // 3. Build a list of valid spawn points from the chunk's vertices
            std::vector<glm::vec3> candidate_points;
            const float kMaxFlatnessDot = 0.95f;
            for (size_t i = 0; i < chunk->vertices.size(); ++i) {
                const auto& vertex = chunk->vertices[i];
                const auto& normal = chunk->normals[i];

                if (vertex.y >= kMinHeight && glm::dot(normal, {0, 1, 0}) > kMaxFlatnessDot) {
                    candidate_points.push_back(vertex);
                }
            }

            // 4. If we have candidates, pick one and spawn a launcher
            if (!candidate_points.empty()) {
                std::uniform_int_distribution<size_t> dist(0, candidate_points.size() - 1);
                const auto& spawn_point = candidate_points[dist(eng_)];
                AddEntity<MissileLauncher>(spawn_point);
                break; // Only spawn one launcher per check
            }
        }
    }

    std::random_device rd_;
    std::mt19937 eng_;
    float time_since_last_spawn_check_;
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
