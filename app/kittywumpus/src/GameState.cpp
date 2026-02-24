#include "GameState.h"

#include "FirstPersonController.h"
#include "KittywumpusInputController.h"
#include "KittywumpusPlane.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include <GLFW/glfw3.h>

namespace Boidsish {

// Static FPS controller instance
static FirstPersonController s_fps_controller;

GameStateManager::GameStateManager() {}

void GameStateManager::Update(
	float dt,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane,
	const KittywumpusInputController& input
) {
	switch (state_) {
	case GameState::MAIN_MENU:
		if (WasAnyKeyPressed(input)) {
			TransitionTo(GameState::FLIGHT_MODE, viz, plane);
		}
		break;

	case GameState::FLIGHT_MODE:
		UpdateFlightMode(dt, viz, plane, input);
		break;

	case GameState::LANDING_TRANSITION:
		UpdateLandingTransition(dt, viz, plane);
		break;

	case GameState::FIRST_PERSON_MODE:
		UpdateFirstPersonMode(dt, viz, plane, input);
		break;

	case GameState::TAKEOFF_TRANSITION:
		UpdateTakeoffTransition(dt, viz, plane);
		break;

	case GameState::GAME_OVER:
		if (WasAnyKeyPressed(input)) {
			TransitionTo(GameState::MAIN_MENU, viz, plane);
		}
		break;
	}
}

void GameStateManager::SetupMainMenu(Visualizer& viz) {
	// Position camera for scenic view
	viz.SetCameraMode(CameraMode::STATIONARY);
	auto& cam = viz.GetCamera();
	cam.x = 0.0f;
	cam.y = 100.0f;
	cam.z = 200.0f;
	cam.pitch = -15.0f;
	cam.yaw = 180.0f;

	// Title and prompt will be added by main.cpp
}

void GameStateManager::SetupGameOver(Visualizer& viz, int final_score) {
	(void)viz;
	(void)final_score;
	// Messages will be added by the handler
}

float GameStateManager::GetTakeoffChargeProgress() const {
	return takeoff_charge_ / kTakeoffChargeRequired;
}

std::shared_ptr<Model> GameStateManager::GetFPSRigModel() const {
	if (state_ == GameState::FIRST_PERSON_MODE) {
		return s_fps_controller.GetRigModel();
	}
	return nullptr;
}

bool GameStateManager::WasAnyKeyPressed(const KittywumpusInputController& input) const {
	return input.any_key_pressed;
}

void GameStateManager::TransitionTo(
	GameState new_state,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane
) {
	// Exit current state
	switch (state_) {
	case GameState::FIRST_PERSON_MODE:
		s_fps_controller.Shutdown(viz);
		break;
	default:
		break;
	}

	// Enter new state
	state_ = new_state;
	transition_time_ = 0.0f;
	takeoff_charge_ = 0.0f;

	switch (new_state) {
	case GameState::MAIN_MENU:
		SetupMainMenu(viz);
		break;

	case GameState::FLIGHT_MODE:
		viz.SetCameraMode(CameraMode::CHASE);
		if (plane) {
			viz.SetChaseCamera(plane);
		}
		glfwSetInputMode(viz.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		break;

	case GameState::LANDING_TRANSITION: {
		// Store starting camera position for interpolation
		auto& cam = viz.GetCamera();
		transition_start_pos_ = cam.pos();
		transition_start_yaw_ = cam.yaw;
		transition_start_pitch_ = cam.pitch;

		// Calculate landing position
		if (plane) {
			auto plane_pos = plane->GetPosition().Toglm();
			auto [h, norm] = viz.GetTerrainPropertiesAtPoint(plane_pos.x, plane_pos.z);
			(void)norm;
			transition_end_pos_ = glm::vec3(plane_pos.x, h + 1.7f, plane_pos.z);
		}
		break;
	}

	case GameState::FIRST_PERSON_MODE:
		if (plane) {
			auto plane_pos = plane->GetPosition().Toglm();
			auto [h, norm] = viz.GetTerrainPropertiesAtPoint(plane_pos.x, plane_pos.z);
			(void)norm;
			glm::vec3 fps_pos(plane_pos.x, h, plane_pos.z);

			// Get yaw from plane orientation
			auto fwd = plane->GetOrientation() * glm::vec3(0, 0, -1);
			float yaw = glm::degrees(atan2(fwd.x, -fwd.z));

			s_fps_controller.Initialize(viz, fps_pos, yaw);
		}
		break;

	case GameState::TAKEOFF_TRANSITION: {
		// Store starting camera for transition
		auto& cam = viz.GetCamera();
		transition_start_pos_ = cam.pos();
		transition_start_yaw_ = cam.yaw;
		transition_start_pitch_ = cam.pitch;
		break;
	}

	case GameState::GAME_OVER:
		// Messages added by handler's OnPlaneDeath
		break;
	}
}

void GameStateManager::UpdateFlightMode(
	float dt,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane,
	const KittywumpusInputController& input
) {
	(void)dt;

	if (!plane) return;

	// Check for death
	if (plane->GetHealth() <= 0 && plane->GetPlaneState() == KittywumpusPlane::PlaneState::DEAD) {
		TransitionTo(GameState::GAME_OVER, viz, plane);
		return;
	}

	// Check for landing conditions
	auto pos = plane->GetPosition().Toglm();
	auto [height, norm] = viz.GetTerrainPropertiesAtPoint(pos.x, pos.z);
	(void)norm;
	float height_above_ground = pos.y - height;

	if (input.holding_land_key && height_above_ground < kLandingHeightThreshold) {
		// Begin landing
		plane->BeginLanding();
		TransitionTo(GameState::LANDING_TRANSITION, viz, plane);
	}
}

void GameStateManager::UpdateLandingTransition(
	float dt,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane
) {
	transition_time_ += dt;
	float t = glm::clamp(transition_time_ / kLandingTransitionDuration, 0.0f, 1.0f);

	// Smooth interpolation using smoothstep
	float smooth_t = glm::smoothstep(0.0f, 1.0f, t);

	// Interpolate camera position
	auto& cam = viz.GetCamera();
	glm::vec3 current_pos = glm::mix(transition_start_pos_, transition_end_pos_, smooth_t);
	cam.x = current_pos.x;
	cam.y = current_pos.y;
	cam.z = current_pos.z;

	// Interpolate camera to look forward (pitch to 0, yaw to plane forward)
	if (plane) {
		auto fwd = plane->GetOrientation() * glm::vec3(0, 0, -1);
		float target_yaw = glm::degrees(atan2(fwd.x, -fwd.z));

		// Handle yaw wraparound
		float yaw_diff = target_yaw - transition_start_yaw_;
		if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
		if (yaw_diff < -180.0f) yaw_diff += 360.0f;

		cam.yaw = transition_start_yaw_ + yaw_diff * smooth_t;
		cam.pitch = glm::mix(transition_start_pitch_, 0.0f, smooth_t);
	}

	// Transition complete
	if (transition_time_ >= kLandingTransitionDuration) {
		TransitionTo(GameState::FIRST_PERSON_MODE, viz, plane);
	}
}

void GameStateManager::UpdateFirstPersonMode(
	float dt,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane,
	const KittywumpusInputController& input
) {
	// Update FPS controller
	s_fps_controller.Update(viz, input, dt);

	// Update plane position to match player (for enemies/world logic)
	if (plane) {
		auto fps_pos = s_fps_controller.GetPosition();
		plane->SetLandedPosition(fps_pos);
	}

	// Check for takeoff charge
	if (input.holding_takeoff_key) {
		takeoff_charge_ += dt;

		// INTEGRATION_POINT: Add visual/audio feedback for takeoff charge
		// (e.g., screen intensity, charge bar, engine rev sound)

		if (takeoff_charge_ >= kTakeoffChargeRequired) {
			// Begin takeoff
			if (plane) {
				plane->BeginTakeoff(s_fps_controller.GetYaw(), viz);
			}
			TransitionTo(GameState::TAKEOFF_TRANSITION, viz, plane);
		}
	} else {
		// Reset charge if released early
		takeoff_charge_ = 0.0f;
	}
}

void GameStateManager::UpdateTakeoffTransition(
	float dt,
	Visualizer& viz,
	std::shared_ptr<KittywumpusPlane> plane
) {
	transition_time_ += dt;
	float t = glm::clamp(transition_time_ / kTakeoffTransitionDuration, 0.0f, 1.0f);
	float smooth_t = glm::smoothstep(0.0f, 1.0f, t);

	// Interpolate camera back to chase position
	if (plane) {
		auto& cam = viz.GetCamera();

		// Get chase camera target position (behind and above plane)
		auto plane_pos = plane->GetPosition().Toglm();
		auto plane_fwd = plane->GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 chase_pos = plane_pos - plane_fwd * cam.follow_distance +
		                      glm::vec3(0, cam.follow_elevation, 0);

		glm::vec3 current_pos = glm::mix(transition_start_pos_, chase_pos, smooth_t);
		cam.x = current_pos.x;
		cam.y = current_pos.y;
		cam.z = current_pos.z;

		// Calculate look direction to plane
		glm::vec3 look_dir = plane_pos - current_pos;
		if (glm::length(look_dir) > 0.001f) {
			look_dir = glm::normalize(look_dir);
			float target_yaw = glm::degrees(atan2(look_dir.x, -look_dir.z));
			float target_pitch = glm::degrees(asin(-look_dir.y));

			float yaw_diff = target_yaw - transition_start_yaw_;
			if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
			if (yaw_diff < -180.0f) yaw_diff += 360.0f;

			cam.yaw = transition_start_yaw_ + yaw_diff * smooth_t;
			cam.pitch = glm::mix(transition_start_pitch_, target_pitch, smooth_t);
		}
	}

	// Transition complete
	if (transition_time_ >= kTakeoffTransitionDuration) {
		TransitionTo(GameState::FLIGHT_MODE, viz, plane);
	}
}

} // namespace Boidsish
