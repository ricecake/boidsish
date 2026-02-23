#include <algorithm>
#include <iostream>
#include <memory>

#include "FPSRig.h"
#include "graphics.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		// Initialize the visualizer
		Visualizer viz(1024, 768, "First Person Example");

		// Set up terrain for the environment
		auto terrain = viz.GetTerrain();
		if (terrain) {
			terrain->SetWorldScale(2.0f); // Make the world a bit larger
		}

		// Initialize FPS Rig with the teapot as a placeholder for a weapon/tool
		// We use the Utah Teapot because it's a classic computer graphics primitive.
		auto rig = std::make_shared<FPSRig>("assets/utah_teapot.obj");

		// Movement settings
		const float walkSpeed = 6.0f;
		const float sprintSpeed = 12.0f;
		const float mouseSensitivity = 0.15f;
		const float eyeHeight = 1.7f; // Meters above ground

		// Animation state
		float bobCycle = 0.0f;
		float bobAmount = 0.0f;
		float lastBobSin = 0.0f;

		// Set camera mode to STATIONARY to take full control over movement
		// but manually disable the cursor for a first-person feel.
		viz.SetCameraMode(CameraMode::STATIONARY);
		glfwSetInputMode(viz.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		// Set initial camera position
		auto& cam = viz.GetCamera();
		cam.x = 0.0f;
		cam.z = 0.0f;
		cam.y = eyeHeight;

		// Input state for charging
		float rightHoldTime = 0.0f;
		float leftHoldTime = 0.0f;
		bool  rightDown = false;
		bool  leftDown = false;

		// Input callback to handle movement, bobbing, and footsteps
		viz.AddInputCallback([&](const InputState& state) {
			auto& camera = viz.GetCamera();
			float dt = state.delta_time;

			// 1. Mouse Look
			camera.yaw += static_cast<float>(state.mouse_delta_x) * mouseSensitivity;
			camera.pitch += static_cast<float>(state.mouse_delta_y) * mouseSensitivity;

			// Clamp pitch to avoid flipping
			if (camera.pitch > 89.0f)
				camera.pitch = 89.0f;
			if (camera.pitch < -89.0f)
				camera.pitch = -89.0f;

			// 2. Movement
			bool  isSprinting = state.keys[GLFW_KEY_LEFT_SHIFT];
			float currentSpeed = isSprinting ? sprintSpeed : walkSpeed;

			// Calculate forward and right vectors on the horizontal plane
			glm::vec3 front = camera.front();
			front.y = 0.0f; // Constrain movement to horizontal plane
			if (glm::length(front) > 0.001f) {
				front = glm::normalize(front);
			}
			glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

			glm::vec3 moveDir(0.0f);
			if (state.keys[GLFW_KEY_W])
				moveDir += front;
			if (state.keys[GLFW_KEY_S])
				moveDir -= front;
			if (state.keys[GLFW_KEY_A])
				moveDir -= right;
			if (state.keys[GLFW_KEY_D])
				moveDir += right;

			bool isMoving = glm::length(moveDir) > 0.001f;
			if (isMoving) {
				moveDir = glm::normalize(moveDir);
				camera.x += moveDir.x * currentSpeed * dt;
				camera.z += moveDir.z * currentSpeed * dt;

				// Update bobbing cycle based on speed
				float cycleSpeed = isSprinting ? 12.0f : 8.0f;
				bobCycle += dt * cycleSpeed;

				// Increase bob amount toward target
				float targetBob = isSprinting ? 1.0f : 0.6f;
				bobAmount = glm::mix(bobAmount, targetBob, dt * 5.0f);
			} else {
				// Fade out bobbing when standing still
				bobAmount = glm::mix(bobAmount, 0.0f, dt * 5.0f);
				// Slowly reset cycle to 0 to avoid jumping when starting to move again
				// (Optional: could just let it stay at current value)
			}

			// 3. Footstep Sounds
			// Trigger a sound when the bob cycle reaches its peaks (left/right foot)
			float currentBobSin = sin(bobCycle);
			if ((lastBobSin < 0.95f && currentBobSin >= 0.95f) || (lastBobSin > -0.95f && currentBobSin <= -0.95f)) {
				// Play a footstep sound at the camera position
				// Using test_sound.wav as a placeholder for a footstep
				viz.AddSoundEffect("assets/test_sound.wav", camera.pos(), glm::vec3(0.0f), 0.2f);
			}
			lastBobSin = currentBobSin;

			// 4. Ground Clamping
			// Get terrain height at current position and set camera height
			auto [terrainHeight, terrainNormal] = viz.GetTerrainPropertiesAtPoint(camera.x, camera.z);
			float targetHeight = terrainHeight + eyeHeight;

			// Apply bobbing to the camera height for extra realism
			targetHeight += sin(bobCycle * 2.0f) * bobAmount * 0.04f;

			// Smoothly interpolate height to avoid jitter on steep slopes
			camera.y = glm::mix(camera.y, targetHeight, dt * 15.0f);

			// 5. Update FPS Rig (View Model)
			// Pass mouse deltas to the rig for sway effect
			rig->Update(
				camera.pos(),
				camera.front(),
				camera.up(),
				dt,
				bobAmount,
				bobCycle,
				static_cast<float>(state.mouse_delta_x),
				static_cast<float>(state.mouse_delta_y)
			);

			// 6. Handle Charging and Explosions
			// Right Click - Explosion
			if (state.mouse_buttons[GLFW_MOUSE_BUTTON_RIGHT]) {
				rightHoldTime += dt;
				rightDown = true;
			} else if (rightDown && state.mouse_button_up[GLFW_MOUSE_BUTTON_RIGHT]) {
				int width, height;
				glfwGetWindowSize(viz.GetWindow(), &width, &height);
				auto target = viz.ScreenToWorld(width / 2.0, height / 2.0);
				if (target) {
					float intensity = 1.0f + rightHoldTime * 2.0f;
					viz.CreateExplosion(*target, intensity);
					viz.AddSoundEffect("assets/rocket_explosion.wav", *target, {0, 0, 0}, glm::min(intensity, 5.0f));
				}
				rightHoldTime = 0.0f;
				rightDown = false;
			}

			// Left Click - Glitter
			if (state.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT]) {
				leftHoldTime += dt;
				leftDown = true;
			} else if (leftDown && state.mouse_button_up[GLFW_MOUSE_BUTTON_LEFT]) {
				int width, height;
				glfwGetWindowSize(viz.GetWindow(), &width, &height);
				auto target = viz.ScreenToWorld(width / 2.0, height / 2.0);
				if (target) {
					float intensity = 1.0f + leftHoldTime * 2.0f;
					// Custom Glitter Explosion
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
				}
				leftHoldTime = 0.0f;
				leftDown = false;
			}

			// Update SuperSpeed Intensity
			viz.SetSuperSpeedIntensity(glm::min(std::max(rightHoldTime, leftHoldTime), 1.0f));
		});

		// 6. Shape Handler
		// Add the rig's model to the scene every frame
		viz.AddShapeHandler([&](float) {
			std::vector<std::shared_ptr<Shape>> shapes;
			if (rig && rig->GetModel()) {
				shapes.push_back(rig->GetModel());
			}
			return shapes;
		});

		// 7. UI / HUD
		viz.AddHudCompass();
		viz.AddHudLocation();
		viz.AddHudMessage("First Person Demo", HudAlignment::TOP_CENTER, {0, 10}, 1.5f);
		viz.AddHudMessage("WASD to Move | SHIFT to Sprint", HudAlignment::BOTTOM_CENTER, {0, -20}, 1.0f);
		viz.AddHudMessage("+", HudAlignment::MIDDLE_CENTER, {0, 0}, 1.0f);

		// Run the simulation
		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
