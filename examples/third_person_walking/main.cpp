#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "procedural_walking_creature.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

int main(int argc, char** argv) {
	try {
		Visualizer vis(1280, 960, "Third Person Procedural Walking Demo");

		// Create the procedural walking creature
		auto creature = std::make_shared<ProceduralWalkingCreature>(0, 0, 0, 0, 12.0f);
		creature->SetClampedToTerrain(true);
		vis.AddShape(creature);

		// Use TRACKING mode for an orbiting third-person camera
		vis.SetCameraMode(CameraMode::TRACKING);

		// Setup initial tracking state
		auto& camera = vis.GetCamera();
		camera.follow_distance = 30.0f;
		camera.follow_elevation = 10.0f;

		// Add an input callback for steering
		vis.AddInputCallback([&](const InputState& state) {
			auto& cam = vis.GetCamera();

			// Get camera's horizontal forward and right vectors
			glm::vec3 cam_front = cam.front();
			glm::vec3 forward = glm::normalize(glm::vec3(cam_front.x, 0.0f, cam_front.z));
			glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

			glm::vec3 move_dir(0.0f);
			if (state.keys[GLFW_KEY_W]) move_dir += forward;
			if (state.keys[GLFW_KEY_S]) move_dir -= forward;
			if (state.keys[GLFW_KEY_A]) move_dir -= right;
			if (state.keys[GLFW_KEY_D]) move_dir += right;

			if (glm::length(move_dir) > 0.001f) {
				move_dir = glm::normalize(move_dir);
				// The creature's SetTarget handles rotation and movement speed
				glm::vec3 target = glm::vec3(creature->GetX(), creature->GetY(), creature->GetZ()) + move_dir * 5.0f;
				creature->SetTarget(target);
			} else {
				// If not moving, set target to current position to stop
				creature->SetTarget(glm::vec3(creature->GetX(), creature->GetY(), creature->GetZ()));
			}

			// Point head where camera is pointing
			creature->SetLookDirection(cam_front);
		});

		// Update handler to keep camera centered on creature and handle orbits
		vis.AddUpdateHandler([&](float time, float dt) {
			creature->Update(dt);

			// In TRACKING mode, the Visualizer automatically orbits based on mouse input
			// and keeps the camera focused on the "tracked dot index".
			// We need to ensure the tracked object is our creature.
			// However, VisualizerImpl uses 'shapes' vector for tracking.
			// Let's use LookAt as a fallback if needed, but TRACKING mode is preferred.

			// To make TRACKING mode work with a specific shape:
			// 1. We added the shape to the persistent shapes via AddShape
			// 2. We can ensure it's the one being tracked.
			vis.LookAt(creature);
		});

		std::cout << "Use WASD to steer the creature!" << std::endl;
		std::cout << "The camera orbits with the mouse (TRACKING mode)." << std::endl;
		std::cout << "The creature's head points where the camera is looking." << std::endl;

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
