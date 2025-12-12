#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"
#include <glm/glm.hpp>

namespace Boidsish {
	// Camera structure for 3D view control
	struct Camera {
		float x, y, z;    // Camera position
		float pitch, yaw; // Camera rotation
		float fov;        // Field of view

		glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

		constexpr Camera(
			float x = 20.0f,
			float y = 20.0f,
			float z = 50.0f,
			float pitch = -20.0f,
			float yaw = -90.0f,
			float fov = 45.0f
		):
			x(x), y(y), z(z), pitch(pitch), yaw(yaw), fov(fov) {}
	};

	// Main visualization class
	class Visualizer {
	public:
		Visualizer(int width = 800, int height = 600, const char* title = "Boidsish 3D Visualizer");
		~Visualizer();

		// Set the function/handler that generates shapes for each frame
		void AddShapeHandler(ShapeFunction func);
		void ClearShapeHandlers();

		// Legacy method name for compatibility
		void SetDotFunction(ShapeFunction func) { AddShapeHandler(func); }

		// Start the visualization loop
		void Run();

		// Check if the window should close
		bool ShouldClose() const;

		// Update one frame
		void Update();

		// Render one frame
		void Render();

		// Get current camera
		Camera& GetCamera();

		// Set camera position and orientation
		void SetCamera(const Camera& camera);

	private:
		struct VisualizerImpl;
		VisualizerImpl* impl;
	};

} // namespace Boidsish