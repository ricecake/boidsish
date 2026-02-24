#pragma once

#include <memory>

#include "FPSRig.h"
#include <glm/glm.hpp>

namespace Boidsish {

class Visualizer;
struct KittywumpusInputController;

// INTEGRATION_POINT: Extend for future FPS features
// - Add weapon handling (FPS weapons, ammo, reload)
// - Add interaction system (interact with objects, doors, pickups)
// - Add crouch/prone states
class FirstPersonController {
public:
	FirstPersonController();

	// Initialize the controller with a starting position
	void Initialize(Visualizer& viz, const glm::vec3& position, float initial_yaw);

	// Update movement, camera, and FPS rig
	void Update(
		Visualizer& viz,
		const KittywumpusInputController& input,
		float delta_time
	);

	// Clean up (destroy FPSRig, restore cursor)
	void Shutdown(Visualizer& viz);

	// Get current position
	glm::vec3 GetPosition() const { return position_; }

	// Get current yaw (for takeoff orientation)
	float GetYaw() const { return yaw_; }

	// Check if initialized
	bool IsInitialized() const { return initialized_; }

	// Get the FPS rig model for rendering
	std::shared_ptr<Model> GetRigModel() const {
		return fps_rig_ ? fps_rig_->GetModel() : nullptr;
	}

private:
	void UpdateMovement(
		Visualizer& viz,
		const KittywumpusInputController& input,
		float delta_time
	);

	void UpdateCamera(
		Visualizer& viz,
		const KittywumpusInputController& input,
		float delta_time
	);

	void UpdateFPSRig(
		Visualizer& viz,
		float delta_time
	);

	// State
	bool initialized_ = false;
	glm::vec3 position_ = glm::vec3(0.0f);
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;

	// FPS Rig for view model
	std::shared_ptr<FPSRig> fps_rig_;

	// Head bobbing state
	float bob_cycle_ = 0.0f;
	float bob_amount_ = 0.0f;
	float last_bob_sin_ = 0.0f;

	// Weapon effect state (mouse button charging)
	float right_hold_time_ = 0.0f;
	float left_hold_time_ = 0.0f;
	bool right_was_down_ = false;
	bool left_was_down_ = false;

	// Settings
	static constexpr float kWalkSpeed = 6.0f;
	static constexpr float kSprintSpeed = 12.0f;
	static constexpr float kMouseSensitivity = 0.15f;
	static constexpr float kEyeHeight = 1.7f;
};

} // namespace Boidsish
