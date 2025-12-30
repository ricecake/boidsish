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
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>

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
        const int kSpawnAttempts = 10;
        const float kMinSpawnDistFromPlane = 100.0f;
        const float kMaxSpawnDistFromPlane = 450.0f;
        const float kMinHeight = 60.0f;
        const float kMinSpacing = 30.0f;
        const float kMaxFlatnessDot = 0.95f; // Normal dot product with "up" must be > this value

        auto existing_launchers = GetEntitiesByType<MissileLauncher>();

        for (int i = 0; i < kSpawnAttempts; ++i) {
            // 1. Pick a random point around the plane
            std::uniform_real_distribution<float> dist_angle(0, 2 * glm::pi<float>());
            std::uniform_real_distribution<float> dist_dist(kMinSpawnDistFromPlane, kMaxSpawnDistFromPlane);

            float angle = dist_angle(eng_);
            float dist = dist_dist(eng_);

            glm::vec3 plane_pos = {plane->GetPosition().x, plane->GetPosition().y, plane->GetPosition().z};
            glm::vec3 spawn_pos_2d = plane_pos + glm::vec3(cos(angle) * dist, 0, sin(angle) * dist);

            // 2. Get terrain properties at that point
            auto [height, normal] = vis->GetTerrainPointProperties(spawn_pos_2d.x, spawn_pos_2d.z);
            glm::vec3 spawn_pos_3d = {spawn_pos_2d.x, height, spawn_pos_2d.z};

            // 3. Check if the location is suitable
            if (height < kMinHeight) continue;
            if (glm::dot(normal, glm::vec3(0, 1, 0)) < kMaxFlatnessDot) continue;

            // 4. Check if it's too close to an existing launcher
            bool too_close = false;
            for (const auto& launcher : existing_launchers) {
                glm::vec3 launcher_pos = {launcher->GetPosition().x, launcher->GetPosition().y, launcher->GetPosition().z};
                if (glm::distance(spawn_pos_3d, launcher_pos) < kMinSpacing) {
                    too_close = true;
                    break;
                }
            }
            if (too_close) continue;

            // 5. Spawn the launcher
            AddEntity<MissileLauncher>(spawn_pos_3d);
            // After adding one, we can just return to avoid clumping them in one frame
            return;
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
