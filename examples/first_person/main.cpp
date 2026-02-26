#include <algorithm>
#include <iostream>
#include <memory>

#include "FPSRig.h"
#include "graphics.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

// Helper to replicate shader hashing for stable random values
float shader_hash(uint32_t x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return static_cast<float>(x) / 4294967295.0f;
}

// Simplified CPU-side version of the decor placement logic
// Returns a list of nearby tree positions
std::vector<glm::vec2> GetNearbyTrees(const glm::vec3& cameraPos, std::shared_ptr<ITerrainGenerator> terrainGen) {
	std::vector<glm::vec2> trees;
	if (!terrainGen)
		return trees;

	const float worldScale = terrainGen->GetWorldScale();
	const float chunkSize = 32.0f * worldScale;
	const float gridSize = 32.0f;
	const float step = chunkSize / gridSize;

	// Check current and neighboring chunks
	int minChunkX = static_cast<int>(std::floor((cameraPos.x - 10.0f) / chunkSize));
	int maxChunkX = static_cast<int>(std::floor((cameraPos.x + 10.0f) / chunkSize));
	int minChunkZ = static_cast<int>(std::floor((cameraPos.z - 10.0f) / chunkSize));
	int maxChunkZ = static_cast<int>(std::floor((cameraPos.z + 10.0f) / chunkSize));

	for (int cx = minChunkX; cx <= maxChunkX; ++cx) {
		for (int cz = minChunkZ; cz <= maxChunkZ; ++cz) {
			glm::vec2 chunkWorldOffset(cx * chunkSize, cz * chunkSize);

			// Replicate placement logic for each of the 3 tree types in PopulateDefaultDecor
			for (int typeIdx = 0; typeIdx < 3; ++typeIdx) {
				float minDensity, maxDensity, minHeight, maxHeight;

				if (typeIdx == 0) { // Apple Tree
					minDensity = 0.025f; maxDensity = 0.05f; minHeight = 5.0f; maxHeight = 95.0f;
				} else if (typeIdx == 1) { // Dead Tree
					minDensity = 0.05f; maxDensity = 0.075f; minHeight = 30.0f; maxHeight = 95.0f;
				} else { // Tree01
					minDensity = 0.05f; maxDensity = 0.075f; minHeight = 5.0f; maxHeight = 95.0f;
				}

				for (int gx = 0; gx < 32; ++gx) {
					for (int gz = 0; gz < 32; ++gz) {
						glm::vec2 localOffset(static_cast<float>(gx) * step, static_cast<float>(gz) * step);
						glm::vec2 worldPos = chunkWorldOffset + localOffset;

						// Stable seeding
						uint32_t seedX = static_cast<uint32_t>(std::abs(worldPos.x));
						uint32_t seedY = static_cast<uint32_t>(std::abs(worldPos.y));
						uint32_t seed = (seedX * 1973 + seedY * 9277 + static_cast<uint32_t>(typeIdx) * 26699) | 1u;

						// Jitter
						worldPos += glm::vec2(shader_hash(seed), shader_hash(seed + 1234u)) * step * 0.9f;

						// Only check trees near the camera
						float distToCam = glm::distance(worldPos, glm::vec2(cameraPos.x, cameraPos.z));
						if (distToCam > 5.0f) continue;

						// Sample terrain
						auto [height, normal] = terrainGen->GetTerrainPropertiesAtPoint(worldPos.x, worldPos.y);
						if (height < minHeight * worldScale || height > maxHeight * worldScale) continue;

						// Biome check
						float controlValue = terrainGen->GetBiomeControlValue(worldPos.x / worldScale, worldPos.y / worldScale);

						bool biomeMatch = false;
						if (typeIdx == 0 && controlValue > 0.1f && controlValue < 0.6f) biomeMatch = true;
						if (typeIdx == 1 && controlValue > 0.4f && controlValue < 0.8f) biomeMatch = true;
						if (typeIdx == 2 && controlValue > 0.1f && controlValue < 0.5f) biomeMatch = true;

						if (!biomeMatch) continue;

						// Density check
						float noiseVal = Simplex::noise(worldPos / (worldScale * 50.0f));
						float combinedNoise = (noiseVal + 1.0f) * 0.5f;
						float effectiveDensity = glm::mix(minDensity, maxDensity, combinedNoise);

						if (shader_hash(seed + 5678) <= effectiveDensity) {
							trees.push_back(worldPos);
						}
					}
				}
			}
		}
	}
	return trees;
}

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
				float nextX = camera.x + moveDir.x * currentSpeed * dt;
				float nextZ = camera.z + moveDir.z * currentSpeed * dt;

				// Collision detection with trees
				auto nearbyTrees = GetNearbyTrees(glm::vec3(nextX, camera.y, nextZ), terrain);
				const float collisionRadius = 0.8f; // Radius of the player capsule

				for (const auto& treePos : nearbyTrees) {
					float dist = glm::distance(glm::vec2(nextX, nextZ), treePos);
					if (dist < collisionRadius) {
						// Simple collision response: slide along the obstacle
						glm::vec2 toCamera = glm::normalize(glm::vec2(nextX, nextZ) - treePos);
						glm::vec2 correctedPos = treePos + toCamera * collisionRadius;
						nextX = correctedPos.x;
						nextZ = correctedPos.y;
					}
				}

				camera.x = nextX;
				camera.z = nextZ;

				// Update bobbing cycle based on speed
				float cycleSpeed = isSprinting ? 12.0f : 8.0f;
				bobCycle += dt * cycleSpeed;

				// Increase bob amount toward target
				float targetBob = isSprinting ? 1.0f : 0.6f;
				bobAmount = glm::mix(bobAmount, targetBob, dt * 5.0f);
			} else {
				// Fade out bobbing when standing still
				bobAmount = glm::mix(bobAmount, 0.0f, dt * 5.0f);
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
			// Average terrain height around current position for smoother movement
			const float sampleRadius = 0.5f;
			float       totalHeight = 0.0f;

			// Sample 5 points (center and 4 around it)
			float h0 = std::get<0>(viz.GetTerrainPropertiesAtPoint(camera.x, camera.z));
			float h1 = std::get<0>(viz.GetTerrainPropertiesAtPoint(camera.x + sampleRadius, camera.z));
			float h2 = std::get<0>(viz.GetTerrainPropertiesAtPoint(camera.x - sampleRadius, camera.z));
			float h3 = std::get<0>(viz.GetTerrainPropertiesAtPoint(camera.x, camera.z + sampleRadius));
			float h4 = std::get<0>(viz.GetTerrainPropertiesAtPoint(camera.x, camera.z - sampleRadius));

			totalHeight = (h0 + h1 + h2 + h3 + h4) / 5.0f;
			float targetHeight = totalHeight + eyeHeight;

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
