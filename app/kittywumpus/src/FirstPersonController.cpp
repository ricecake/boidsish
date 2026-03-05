#include "FirstPersonController.h"

#include <algorithm>

#include "KittywumpusInputController.h"
#include "graphics.h"
#include "light.h"
#include "KittywumpusHandler.h"
#include "CatMissile.h"
#include <GLFW/glfw3.h>

namespace Boidsish {

extern int kittywumpus_selected_weapon;

FirstPersonController::FirstPersonController() {}

void FirstPersonController::Initialize(Visualizer& viz, KittywumpusHandler& handler, const glm::vec3& position, float initial_yaw) {
	(void)handler;
	position_ = position;
	yaw_ = initial_yaw;
	pitch_ = 0.0f;

	// Create FPS Rig with teapot as placeholder weapon/tool
	// INTEGRATION_POINT: Replace with actual weapon model when FPS weapons are implemented
	fps_rig_ = std::make_shared<FPSRig>("assets/utah_teapot.obj");

	// Set camera mode to stationary (we control it manually)
	viz.SetCameraMode(CameraMode::STATIONARY);

	// Disable cursor for FPS look
	glfwSetInputMode(viz.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set initial camera position
	auto& cam = viz.GetCamera();
	cam.x = position_.x;
	cam.y = position_.y + kEyeHeight;
	cam.z = position_.z;
	cam.yaw = yaw_;
	cam.pitch = pitch_;

	// Initialize ground height smoothing
	auto [initial_terrain_height, initial_terrain_normal] = viz.GetTerrainPropertiesAtPoint(position_.x, position_.z);
	(void)initial_terrain_normal;
	smoothed_ground_height_ = initial_terrain_height;

	// Reset bobbing state
	bob_cycle_ = 0.0f;
	bob_amount_ = 0.0f;
	last_bob_sin_ = 0.0f;

	initialized_ = true;
}

void FirstPersonController::Update(
	Visualizer& viz,
	KittywumpusHandler& handler,
	const KittywumpusInputController& input,
	float delta_time
) {
	if (!initialized_) return;

	UpdateCamera(viz, input, delta_time);
	UpdateMovement(viz, input, delta_time);
	UpdateFPSRig(viz, delta_time);

	if (fire_cooldown_ > 0) {
		fire_cooldown_ -= delta_time;
	}

	// Handle weapon effects (mouse button charging)
	// Right Click - Explosion
	if (input.mouse_right) {
		right_hold_time_ += delta_time;
		right_was_down_ = true;
	} else if (right_was_down_ && input.mouse_right_released) {
		int width, height;
		glfwGetWindowSize(viz.GetWindow(), &width, &height);
		auto target = viz.ScreenToWorld(width / 2.0, height / 2.0);
		if (target) {
			float intensity = 1.0f + right_hold_time_ * 2.0f;
			viz.CreateExplosion(*target, intensity);
			viz.AddSoundEffect("assets/rocket_explosion.wav", *target, {0, 0, 0}, glm::min(intensity, 5.0f));
			handler.TriggerRadiusDamage(*target, 10.0f * intensity, 50.0f * intensity);
		}
		right_hold_time_ = 0.0f;
		right_was_down_ = false;
	}

	// Left Click - Glitter / Selected Weapon
	if (input.mouse_left) {
		if (kittywumpus_selected_weapon == 0) { // CatMissile
			if (fire_cooldown_ <= 0) {
				auto cam_pos = viz.GetCamera().pos();
				auto cam_fwd = viz.GetCamera().front();
				auto cam_up = viz.GetCamera().up();
				auto cam_right = glm::normalize(glm::cross(cam_fwd, cam_up));

				glm::quat orient = glm::quatLookAt(cam_fwd, cam_up);
				glm::vec3 spawn_pos = cam_pos + cam_fwd * 2.0f + cam_right * 0.5f;
				glm::vec3 initial_vel = cam_fwd * 20.0f;
				handler.QueueAddEntity<CatMissile>(
					Vector3(spawn_pos.x, spawn_pos.y, spawn_pos.z),
					orient,
					glm::vec3(0, 0, -1),
					Vector3(initial_vel.x, initial_vel.y, initial_vel.z),
					false
				);
				fire_cooldown_ = 0.75f;
			}
		} else {
			left_hold_time_ += delta_time;
			left_was_down_ = true;
		}
	} else if (left_was_down_ && input.mouse_left_released) {
		int width, height;
		glfwGetWindowSize(viz.GetWindow(), &width, &height);
		auto target = viz.ScreenToWorld(width / 2.0, height / 2.0);
		if (target) {
			float intensity = 1.0f + left_hold_time_ * 2.0f;
			// Glitter effect
			viz.AddFireEffect(
				*target,
				FireEffectStyle::Glitter,
				{0, 0, 0},
				{0, 0, 0},
				static_cast<int>(500 * intensity),
				0.5f
			);
			viz.CreateShockwave(*target, intensity, 30.0f * intensity, 1.5f, {0, 1, 0}, {0.8f, 0.2f, 1.0f});

			Light flash = Light::CreateFlash(*target, 45.0f * intensity, {0.8f, 0.5f, 1.0f}, 45.0f * intensity);
			flash.auto_remove = true;
			flash.SetEaseOut(0.4f * intensity);
			viz.GetLightManager().AddLight(flash);

			viz.AddSoundEffect("assets/rocket_explosion.wav", *target, {0, 0, 0}, glm::min(intensity, 5.0f));
			handler.TriggerRadiusDamage(*target, 8.0f * intensity, 30.0f * intensity);
		}
		left_hold_time_ = 0.0f;
		left_was_down_ = false;
	}

	// Update SuperSpeed intensity for visual feedback during charging
	viz.SetSuperSpeedIntensity(glm::min(std::max(right_hold_time_, left_hold_time_), 1.0f));
}

void FirstPersonController::UpdateCamera(
	Visualizer& viz,
	const KittywumpusInputController& input,
	float delta_time
) {
	(void)delta_time;
	auto& camera = viz.GetCamera();

	// Mouse look
	yaw_ += input.mouse_delta_x * kMouseSensitivity;
	pitch_ += input.mouse_delta_y * kMouseSensitivity;

	// Clamp pitch to avoid flipping
	if (pitch_ > 89.0f) pitch_ = 89.0f;
	if (pitch_ < -89.0f) pitch_ = -89.0f;

	camera.yaw = yaw_;
	camera.pitch = pitch_;
}

void FirstPersonController::UpdateMovement(
	Visualizer& viz,
	const KittywumpusInputController& input,
	float delta_time
) {
	auto& camera = viz.GetCamera();

	bool is_sprinting = input.sprint;
	float current_speed = is_sprinting ? kSprintSpeed : kWalkSpeed;

	// Calculate forward and right vectors on the horizontal plane
	glm::vec3 front = camera.front();
	front.y = 0.0f; // Constrain movement to horizontal plane
	if (glm::length(front) > 0.001f) {
		front = glm::normalize(front);
	}
	glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

	glm::vec3 move_dir(0.0f);
	if (input.move_forward) move_dir += front;
	if (input.move_backward) move_dir -= front;
	if (input.move_left) move_dir -= right;
	if (input.move_right) move_dir += right;

	bool is_moving = glm::length(move_dir) > 0.001f;
	if (is_moving) {
		move_dir = glm::normalize(move_dir);
		position_.x += move_dir.x * current_speed * delta_time;
		position_.z += move_dir.z * current_speed * delta_time;

		// Update bobbing cycle based on speed
		float cycle_speed = is_sprinting ? 12.0f : 8.0f;
		bob_cycle_ += delta_time * cycle_speed;

		// Increase bob amount toward target
		float target_bob = is_sprinting ? 1.0f : 0.6f;
		bob_amount_ = glm::mix(bob_amount_, target_bob, delta_time * 5.0f);
	} else {
		// Fade out bobbing when standing still
		bob_amount_ = glm::mix(bob_amount_, 0.0f, delta_time * 5.0f);
	}

	// Footstep sounds
	float current_bob_sin = sin(bob_cycle_);
	if ((last_bob_sin_ < 0.95f && current_bob_sin >= 0.95f) ||
	    (last_bob_sin_ > -0.95f && current_bob_sin <= -0.95f)) {
		viz.AddSoundEffect("assets/test_sound.wav", camera.pos(), glm::vec3(0.0f), 0.2f);
	}
	last_bob_sin_ = current_bob_sin;

	// Ground clamping with spatial and temporal smoothing
	float h0 = std::get<0>(viz.GetTerrainPropertiesAtPoint(position_.x, position_.z));
	float h1 = std::get<0>(viz.GetTerrainPropertiesAtPoint(position_.x + 0.4f, position_.z));
	float h2 = std::get<0>(viz.GetTerrainPropertiesAtPoint(position_.x - 0.4f, position_.z));
	float h3 = std::get<0>(viz.GetTerrainPropertiesAtPoint(position_.x, position_.z + 0.4f));
	float h4 = std::get<0>(viz.GetTerrainPropertiesAtPoint(position_.x, position_.z - 0.4f));
	float avg_terrain_height = (h0 + h1 + h2 + h3 + h4) / 5.0f;

	// Smoothly interpolate ground height (temporal smoothing)
	smoothed_ground_height_ = glm::mix(smoothed_ground_height_, avg_terrain_height, delta_time * 5.0f);

	float target_height = smoothed_ground_height_ + kEyeHeight;

	// Apply bobbing to camera height
	target_height += sin(bob_cycle_ * 2.0f) * bob_amount_ * 0.04f;

	// Smoothly interpolate height
	position_.y = glm::mix(position_.y, smoothed_ground_height_, delta_time * 15.0f);
	camera.x = position_.x;
	camera.y = glm::mix(camera.y, target_height, delta_time * 15.0f);
	camera.z = position_.z;
}

void FirstPersonController::UpdateFPSRig(
	Visualizer& viz,
	float delta_time
) {
	if (!fps_rig_) return;

	auto& camera = viz.GetCamera();
	fps_rig_->Update(
		camera.pos(),
		camera.front(),
		camera.up(),
		delta_time,
		bob_amount_,
		bob_cycle_,
		0.0f, // Mouse delta already applied to camera
		0.0f
	);
}

void FirstPersonController::Shutdown(Visualizer& viz) {
	if (!initialized_) return;

	// Restore cursor
	glfwSetInputMode(viz.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Clear FPS rig
	fps_rig_.reset();

	initialized_ = false;
}

} // namespace Boidsish
