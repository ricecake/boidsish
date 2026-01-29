#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "Config.h"
#include "audio_manager.h"
#include "concurrent_queue.h"
#include "fire_effect.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "light_manager.h"
#include "shape.h"
#include "vector.h"
#include "visual_effects.h"
#include <glm/glm.hpp>

namespace task_thread_pool {
	class task_thread_pool;
}

namespace Boidsish {
	struct HudIcon;
	struct HudNumber;
	struct HudGauge;

	// class AudioManager;

	namespace UI {
		class IWidget;
	}
	class EntityBase;
	class FireEffect;
	class SoundEffect;
	class FireEffectManager;
	class ShockwaveManager;
	class DecorManager;
	class Path;
} // namespace Boidsish

namespace Boidsish {
	enum class ShapeCommandType { Add, Remove };

	struct ShapeCommand {
		ShapeCommandType       type;
		std::shared_ptr<Shape> shape;
		int                    shape_id;
	};

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
		bool   mouse_buttons[8];
		bool   mouse_button_down[8];
		bool   mouse_button_up[8];
		float  delta_time;
	};

	enum class CameraMode { FREE, AUTO, TRACKING, STATIONARY, CHASE, PATH_FOLLOW };

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
	class TerrainGenerator;

	// Main visualization class
	class Visualizer {
	public:
		Visualizer(int w, int h, const char* title);
		~Visualizer();

		// Set the function/handler that generates shapes for each frame
		void AddShapeHandler(ShapeFunction func);
		void ClearShapeHandlers();

		void AddShape(std::shared_ptr<Shape> shape);
		void RemoveShape(int shape_id);

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
		Camera&       GetCamera();
		const Camera& GetCamera() const;

		glm::mat4   GetProjectionMatrix() const;
		glm::mat4   GetViewMatrix() const;
		GLFWwindow* GetWindow() const;

		// Set camera position and orientation
		void SetCamera(const Camera& camera);

		// Add an input callback to the chain of handlers.
		void AddInputCallback(InputCallback callback);

		std::optional<glm::vec3> ScreenToWorld(double screen_x, double screen_y) const;

		void SetChaseCamera(std::shared_ptr<EntityBase> target);
		void SetPathCamera(std::shared_ptr<Path> path);

		// Add a UI widget to be rendered
		void AddWidget(std::shared_ptr<UI::IWidget> widget);

		// Set the exit key, which cannot be overridden by the input callback.
		void SetExitKey(int key);

		CameraMode                  GetCameraMode() const;
		void                        SetCameraMode(CameraMode mode);
		void                        TogglePause();
		void                        SetPause(bool);
		bool                        GetPause();
		void                        SetTimeScale(float);
		float                       GetTimeScale();
		void                        ToggleEffect(VisualEffect effect);
		void                        ToggleMenus();
		std::shared_ptr<FireEffect> AddFireEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f
		);
		void                         RemoveFireEffect(const std::shared_ptr<FireEffect>& effect) const;
		std::shared_ptr<SoundEffect> AddSoundEffect(
			const std::string& filepath,
			const glm::vec3&   position,
			const glm::vec3&   velocity = glm::vec3(0.0f),
			float              volume = 1.0f,
			bool               loop = false,
			float              lifetime = -1.0f
		);
		void RemoveSoundEffect(const std::shared_ptr<SoundEffect>& effect) const;
		void TogglePostProcessingEffect(const std::string& name);
		void TogglePostProcessingEffect(const std::string& name, const bool newState);
		void SetFilmGrainIntensity(float intensity);

		// Shockwave effect methods
		/**
		 * @brief Add a shockwave effect at the given position.
		 *
		 * Creates a dramatic expanding ring distortion effect, ideal for explosions.
		 *
		 * @param position World-space center of the shockwave
		 * @param max_radius Maximum radius the wave will expand to (world units)
		 * @param duration Time for the wave to reach max_radius (seconds)
		 * @param intensity Distortion strength (0.0 to 1.0, default 0.5)
		 * @param ring_width Width of the distortion ring in world units (default 3.0)
		 * @param color Color tint for the shockwave glow (default orange)
		 * @return true if added, false if at capacity
		 */
		bool AddShockwave(
			const glm::vec3& position,
			const glm::vec3& normal,
			float            max_radius,
			float            duration,
			float            intensity = 0.5f,
			float            ring_width = 3.0f,
			const glm::vec3& color = glm::vec3(1.0f, 0.6f, 0.2f)
		);

		/**
		 * @brief Create an explosion with fire particles and shockwave.
		 *
		 * Convenience method that combines fire effect with shockwave for
		 * a complete explosion visual.
		 *
		 * @param position World-space center of the explosion
		 * @param intensity Scale factor for effect strength (default 1.0)
		 */
		void CreateExplosion(const glm::vec3& position, float intensity = 1.0f);

		std::tuple<float, glm::vec3>                 GetTerrainPointProperties(float x, float y) const;
		std::tuple<float, glm::vec3>                 GetTerrainPointPropertiesThreadSafe(float x, float y) const;
		float                                        GetTerrainMaxHeight() const;
		const TerrainGenerator*                      GetTerrainGenerator() const;
		const std::vector<std::shared_ptr<Terrain>>& GetTerrainChunks() const;
		task_thread_pool::task_thread_pool&          GetThreadPool();
		LightManager&                                GetLightManager();
		FireEffectManager*                           GetFireEffectManager();
		DecorManager*                                GetDecorManager();
		float                                        GetLastFrameTime() const;

		Config&       GetConfig();
		AudioManager& GetAudioManager();

		// HUD methods
		void AddHudIcon(const HudIcon& icon);
		void UpdateHudIcon(int id, const HudIcon& icon);
		void RemoveHudIcon(int id);

		bool IsRippleEffectEnabled() const;
		bool IsColorShiftEffectEnabled() const;
		bool IsBlackAndWhiteEffectEnabled() const;
		bool IsNegativeEffectEnabled() const;
		bool IsShimmeryEffectEnabled() const;
		bool IsGlitchedEffectEnabled() const;
		bool IsWireframeEffectEnabled() const;

		void AddHudNumber(const HudNumber& number);
		void UpdateHudNumber(int id, const HudNumber& number);
		void RemoveHudNumber(int id);

		void AddHudGauge(const HudGauge& gauge);
		void UpdateHudGauge(int id, const HudGauge& gauge);
		void RemoveHudGauge(int id);

	private:
		struct VisualizerImpl;
		std::unique_ptr<VisualizerImpl> impl;
	};

} // namespace Boidsish