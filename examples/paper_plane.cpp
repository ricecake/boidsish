#include <iostream>
#include <memory>
#include <random>
#include <vector>
#include <set>

#include "paper_plane.h"
#include "guided_missile.h"
#include "missile_launcher.h"
#include "graphics.h"
#include "spatial_entity_handler.h"
#include "terrain.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

class PaperPlaneHandler: public SpatialEntityHandler {
public:
	PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool):
		SpatialEntityHandler(thread_pool), eng_(rd_()) {}

	void PreTimestep(float time, float delta_time) override {
        // --- Launcher Lifecycle Management ---
        time_since_last_spawn_check_ += delta_time;
        if (time_since_last_spawn_check_ >= kSpawnCheckInterval_) {
            time_since_last_spawn_check_ = 0.0f;
            ManageLaunchers();
        }
	}

private:
    void ManageLaunchers() {
        auto planes = GetEntitiesByType<PaperPlane>();
        if (planes.empty()) return;
        auto plane = planes[0];
        Vector3 plane_pos = plane->GetPosition();

        // Despawn distant launchers
        auto launchers = GetEntitiesByType<MissileLauncher>();
        for (auto& launcher : launchers) {
            if ((launcher->GetPosition() - plane_pos).Magnitude() > kDespawnDistance_) {
                Vector3 launcher_pos = launcher->GetPosition();
                int chunk_x = static_cast<int>(launcher_pos.x / kChunkSize_);
                int chunk_z = static_cast<int>(launcher_pos.z / kChunkSize_);
                occupied_chunks_.erase({chunk_x, chunk_z});
                QueueRemoveEntity(launcher->GetId());
            }
        }

        // Spawn new launchers
        const auto& visible_chunks = vis->GetTerrainChunks();
        if (visible_chunks.empty()) return;

        for (const auto& chunk : visible_chunks) {
            if (chunk->proxy.maxY < kMinSpawnHeight_) continue;
            if (occupied_chunks_.count({chunk->chunk_x, chunk->chunk_z})) continue;

            for (int i = 0; i < 5; ++i) {
                 std::uniform_real_distribution<float> dist(0.0f, kChunkSize_);
                 float_t test_x = chunk->chunk_x * kChunkSize_ + dist(eng_);
                 float_t test_z = chunk->chunk_z * kChunkSize_ + dist(eng_);

                 auto [height, normal] = vis->GetTerrainPointProperties(test_x, test_z);

                 if (height > kMinSpawnHeight_ && normal.y > 0.95f) {
                    Vector3 spawn_pos(test_x, height, test_z);
                    QueueAddEntity<MissileLauncher>(spawn_pos);
                    occupied_chunks_.insert(std::make_pair(chunk->chunk_x, chunk->chunk_z));
                    return;
                 }
            }
        }
    }

    float time_since_last_spawn_check_ = 0.0f;
    const float kSpawnCheckInterval_ = 1.0f;
    const float kMinSpawnHeight_ = 60.0f;
    const float kDespawnDistance_ = 500.0f;
    const int kChunkSize_ = 32;

    std::set<std::pair<int, int>> occupied_chunks_;

	std::random_device rd_;
	std::mt19937       eng_;
};

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Terrain Demo");

		Boidsish::Camera camera;
		visualizer->SetCamera(camera);
		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();

		visualizer->AddShapeHandler(std::ref(handler));

        auto plane = std::dynamic_pointer_cast<PaperPlane>(handler.GetEntity(id));
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
