#include <iostream>
#include <memory>
#include <vector>

#include "checkpoint_ring.h"
#include "constants.h"
#include "dot.h"
#include "entity.h"
#include "graphics.h"
#include "hud.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

class PlayerEntity: public Entity<Dot> {
public:
	PlayerEntity(): Entity<Dot>() {
		SetSize(2.0f);
		SetColor(1.0f, 1.0f, 1.0f);
		SetTrailLength(100);
		SetTrailIridescence(true);
	}

	void UpdateEntity(const EntityHandler& /*handler*/, float /*time*/, float /*delta_time*/) override {
		UpdateShape();
	}
};

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Checkpoint Ring Demo");

		auto handler = EntityHandler(visualizer->GetThreadPool(), visualizer);

		auto playerId = handler.AddEntity<PlayerEntity>();
		auto player = handler.GetEntity(playerId);
		player->SetPosition(0, 50, 100);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(player);

		auto score = visualizer->AddHudScore();

		// Create checkpoints
		auto callback = [score](float dist, std::shared_ptr<EntityBase> entity) {
			std::cout << "Entity " << entity->GetId() << " passed through ring at distance " << dist << std::endl;
			score->AddScore(100, "Checkpoint Passed!");
		};

		// Gold ring
		int  r1 = handler.AddEntity<CheckpointRing>(15.0f, CheckpointStyle::GOLD, callback);
		auto ring1 = std::dynamic_pointer_cast<CheckpointRing>(handler.GetEntity(r1));
		ring1->SetPosition(handler.GetValidPlacement(glm::vec3(0, 50, 0), 20.0f));
		ring1->RegisterEntity(player);

		// Blue ring, rotated
		int  r2 = handler.AddEntity<CheckpointRing>(15.0f, CheckpointStyle::BLUE, callback);
		auto ring2 = std::dynamic_pointer_cast<CheckpointRing>(handler.GetEntity(r2));
		ring2->SetPosition(handler.GetValidPlacement(glm::vec3(50, 50, -50), 20.0f));
		ring2->SetOrientation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)));
		ring2->RegisterEntity(player);

		// Rainbow ring
		int  r3 = handler.AddEntity<CheckpointRing>(20.0f, CheckpointStyle::RAINBOW, callback);
		auto ring3 = std::dynamic_pointer_cast<CheckpointRing>(handler.GetEntity(r3));
		ring3->SetPosition(handler.GetValidPlacement(glm::vec3(0, 70, -100), 25.0f));
		ring3->RegisterEntity(player);

		// Neon Green ring
		int  r4 = handler.AddEntity<CheckpointRing>(10.0f, CheckpointStyle::NEON_GREEN, callback);
		auto ring4 = std::dynamic_pointer_cast<CheckpointRing>(handler.GetEntity(r4));
		ring4->SetPosition(handler.GetValidPlacement(glm::vec3(-50, 40, -150), 15.0f));
		ring4->RegisterEntity(player);

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			float     speed = 50.0f * state.delta_time;
			Vector3   pos = player->GetPosition();
			glm::vec3 forward = visualizer->GetCamera().front();
			glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

			if (state.keys[GLFW_KEY_W])
				pos = pos + Vector3(forward.x, forward.y, forward.z) * speed;
			if (state.keys[GLFW_KEY_S])
				pos = pos - Vector3(forward.x, forward.y, forward.z) * speed;
			if (state.keys[GLFW_KEY_A])
				pos = pos - Vector3(right.x, right.y, right.z) * speed;
			if (state.keys[GLFW_KEY_D])
				pos = pos + Vector3(right.x, right.y, right.z) * speed;

			player->SetPosition(pos);
			player->SetVelocity(Vector3(forward.x, forward.y, forward.z) * speed / state.delta_time);
		});

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
