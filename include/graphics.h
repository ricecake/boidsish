#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"

namespace Boidsish {
	// Camera structure for 3D view control
	struct Camera {
		float x, y, z;    // Camera position
		float pitch, yaw; // Camera rotation
		float fov;        // Field of view

		constexpr Camera(
			float x_ = 0.0f,
			float y_ = 0.0f,
			float z_ = 5.0f,
			float pitch_ = 0.0f,
			float yaw_ = 0.0f,
			float fov_ = 45.0f
		):
			x(x_), y(y_), z(z_), pitch(pitch_), yaw(yaw_), fov(fov_) {}
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