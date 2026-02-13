#include <iostream>
#include <memory>
#include <vector>

#include "constants.h"
#include "entity.h"
#include "graphics.h"
#include "hud.h"
#include "steering_probe.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

struct DemoInput {
	bool pitch_up = false;
	bool pitch_down = false;
	bool yaw_left = false;
	bool yaw_right = false;
	bool roll_left = false;
	bool roll_right = false;
	bool boost = false;
	bool brake = false;
};

class DemoPlayer: public Entity<Dot> {
public:
	DemoPlayer(int id): Entity<Dot>(id) {
		SetSize(20.0f);
		SetColor(0.0f, 0.8f, 1.0f);
		orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		forward_speed_ = 40.0f;
		SetTrailLength(100);
		shape_->SetInstanced(true);
	}

	void UpdateEntity(const EntityHandler& /*handler*/, float /*time*/, float delta_time) override {
		const float kPitchSpeed = 2.0f;
		const float kYawSpeed = 1.5f;
		const float kRollSpeed = 3.0f;
		const float kDamping = 3.0f;

		glm::vec3 target_rot_velocity(0.0f);
		if (input.pitch_up)
			target_rot_velocity.x += kPitchSpeed;
		if (input.pitch_down)
			target_rot_velocity.x -= kPitchSpeed;
		if (input.yaw_left)
			target_rot_velocity.y += kYawSpeed;
		if (input.yaw_right)
			target_rot_velocity.y -= kYawSpeed;
		if (input.roll_left)
			target_rot_velocity.z += kRollSpeed;
		if (input.roll_right)
			target_rot_velocity.z -= kRollSpeed;

		rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1, 0, 0));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0, 1, 0));
		glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0, 0, 1));
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta * roll_delta);

		if (input.boost)
			forward_speed_ = glm::mix(forward_speed_, 120.0f, 1.0f - exp(-delta_time * 2.0f));
		else if (input.brake)
			forward_speed_ = glm::mix(forward_speed_, 10.0f, 1.0f - exp(-delta_time * 2.0f));
		else
			forward_speed_ = glm::mix(forward_speed_, 60.0f, 1.0f - exp(-delta_time * 1.0f));

		glm::vec3 forward = orientation_ * glm::vec3(0, 0, -1);
		SetVelocity(forward * forward_speed_);
	}

	// void UpdateShape() override {
	// 	Entity<Dot>::UpdateShape();
	// 	if (shape_) shape_->SetRotation(orientation_);
	// }

	DemoInput input;

private:
	glm::quat orientation_;
	glm::vec3 rotational_velocity_{0.0f};
	float     forward_speed_;
};

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Steering Probe Demo");
		auto terrain = visualizer->GetTerrain();
		terrain->SetWorldScale(2.0f);

		auto handler = std::make_shared<EntityHandler>(visualizer->GetThreadPool(), visualizer);
		visualizer->AddShapeHandler(std::ref(*handler));

		auto player_id = handler->AddEntity<DemoPlayer>(45443);
		auto player = std::dynamic_pointer_cast<DemoPlayer>(handler->GetEntity(player_id));
		player->SetPosition(0, 150, 0);

		visualizer->SetChaseCamera(player);

		visualizer->AddInputCallback([&](const InputState& state) {
			player->input.pitch_up = state.keys[GLFW_KEY_S];
			player->input.pitch_down = state.keys[GLFW_KEY_W];
			player->input.yaw_left = state.keys[GLFW_KEY_A];
			player->input.yaw_right = state.keys[GLFW_KEY_D];
			player->input.roll_left = state.keys[GLFW_KEY_Q];
			player->input.roll_right = state.keys[GLFW_KEY_E];
			player->input.boost = state.keys[GLFW_KEY_LEFT_SHIFT];
			player->input.brake = state.keys[GLFW_KEY_LEFT_CONTROL];
		});

		auto sp = std::make_shared<SteeringProbe>(terrain);
		sp->SetPosition({0, 150, -100});

		auto probeDot = std::make_shared<Dot>(329392);
		probeDot->SetSize(80.0f);
		probeDot->SetColor(1.0f, 0.0f, 1.0f);
		probeDot->SetInstanced(true);
		visualizer->AddShape(probeDot);

		auto scoreIndicator = visualizer->AddHudScore();

		float last_time = 0;
		visualizer->AddShapeHandler([&](float time) {
			float dt = time - last_time;
			last_time = time;
			if (dt <= 0)
				return std::vector<std::shared_ptr<Shape>>{};
			if (dt > 0.1f)
				dt = 0.1f;

			sp->Update(dt, player->GetPosition().Toglm(), player->GetVelocity().Toglm());

			// Drop checkpoints and track player
			sp->HandleCheckpoints(dt, *handler, player);

			auto pPos = sp->GetPosition();
			probeDot->SetPosition(pPos.x, pPos.y, pPos.z);

			return std::vector<std::shared_ptr<Shape>>{};
		});

		visualizer->AddHudCompass();
		visualizer->AddHudLocation();
		visualizer->AddHudMessage("Follow the Magenta Probe!", HudAlignment::TOP_CENTER, {0, 50}, 1.5f);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
