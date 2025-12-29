#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "Config.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "shape.h"
#include "vector.h"
#include "visual_effects.h"
#include <glm/glm.hpp>

namespace task_thread_pool {
	class task_thread_pool;
}

namespace Boidsish {
	namespace UI {
		class IWidget;
	}
	class EntityBase;
} // namespace Boidsish

namespace Boidsish {
	constexpr int kMaxKeys = 1024;

	struct Plane {
		glm::vec3 normal;
		float     distance;
	};

	struct Frustum {
		Plane planes[6];
	};

	struct InputState {
		bool   keys[kMaxKeys];
		bool   key_down[kMaxKeys];
		bool   key_up[kMaxKeys];
		double mouse_x, mouse_y;
		double mouse_delta_x, mouse_delta_y;
		float  delta_time;
	};

	enum class CameraMode { FREE, AUTO, TRACKING, STATIONARY, CHASE };

	using InputCallback = std::function<void(const InputState&)>;

	// Camera structure for 3D view control
	struct Camera {
		float x, y, z;          // Camera position
		float pitch, yaw, roll; // Camera rotation
		float fov;              // Field of view
		float speed;

		constexpr Camera(
			float x = 0.0f,
			float y = 0.0f,
			float z = 5.0f,
			float pitch = 0.0f,
			float yaw = 0.0f,
			float roll = 0.0f,
			float fov = 45.0f,
			float speed = 10.0f
		):
			x(x), y(y), z(z), pitch(pitch), yaw(yaw), roll(roll), fov(fov), speed(speed) {}

		glm::vec3 front() const {
			glm::vec3 cameraPos(x, y, z);
			glm::vec3 front;
			front.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
			front.y = sin(glm::radians(pitch));
			front.z = -cos(glm::radians(pitch)) * cos(glm::radians(yaw));
			return glm::normalize(front);
		}

		glm::vec3 up() const {
			auto      front_vec = front();
			glm::vec3 right = glm::normalize(glm::cross(front_vec, glm::vec3(0.0f, 1.0f, 0.0f)));
			glm::vec3 up = glm::normalize(glm::cross(right, front_vec));
			glm::mat4 roll_mat = glm::rotate(glm::mat4(1.0f), glm::radians(roll), front_vec);
			return glm::vec3(roll_mat * glm::vec4(up, 0.0f));
		}

		glm::vec3 pos() const { return glm::vec3(x, y, z); }
	};

	// Main visualization class
	class Terrain;

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

		// Add an input callback to the chain of handlers.
		void AddInputCallback(InputCallback callback);

		void SetChaseCamera(std::shared_ptr<EntityBase> target);

		// Add a UI widget to be rendered
		void AddWidget(std::shared_ptr<UI::IWidget> widget);

		// Set the exit key, which cannot be overridden by the input callback.
		void SetExitKey(int key);

		void SetCameraMode(CameraMode mode);
		void TogglePause();
		void ToggleEffect(VisualEffect effect);

		std::tuple<float, glm::vec3>                 GetTerrainPointProperties(float x, float y) const;
		const std::vector<std::shared_ptr<Terrain>>& GetTerrainChunks() const;
		task_thread_pool::task_thread_pool&          GetThreadPool();

		Config& GetConfig();

	private:
		struct VisualizerImpl;
		std::unique_ptr<VisualizerImpl> impl;
	};

} // namespace Boidsish