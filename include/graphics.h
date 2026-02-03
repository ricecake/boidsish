#pragma once

#include <cmath>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "Config.h"
#include "audio_manager.h"
#include "concurrent_queue.h"
#include "constants.h"
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
	class SdfVolumeManager;
	struct SdfSource;
	class DecorManager;
	class Path;

	namespace PostProcessing {
		class PostProcessingManager;
	}
} // namespace Boidsish

#include "frustum.h"

namespace Boidsish {
	enum class ShapeCommandType { Add, Remove };

	struct ShapeCommand {
		ShapeCommandType       type;
		std::shared_ptr<Shape> shape;
		int                    shape_id;
	};

	struct InputState {
		bool   keys[Constants::Library::Input::MaxKeys()];
		bool   key_down[Constants::Library::Input::MaxKeys()];
		bool   key_up[Constants::Library::Input::MaxKeys()];
		double mouse_x, mouse_y;
		double mouse_delta_x, mouse_delta_y;
		bool   mouse_buttons[Constants::Library::Input::MaxMouseButtons()];
		bool   mouse_button_down[Constants::Library::Input::MaxMouseButtons()];
		bool   mouse_button_up[Constants::Library::Input::MaxMouseButtons()];
		float  delta_time;
	};

	enum class CameraMode { FREE, AUTO, TRACKING, STATIONARY, CHASE, PATH_FOLLOW };

	// Forward declaration for PrepareCallback
	class Visualizer;

	using InputCallback = std::function<void(const InputState&)>;
	using PrepareCallback = std::function<void(Visualizer&)>;

	// Camera structure for 3D view control
	struct Camera {
		float x, y, z;          // Camera position
		float pitch, yaw, roll; // Camera rotation
		float fov;              // Field of view
		float speed;

		// Follow camera settings
		float follow_distance;
		float follow_elevation;
		float follow_look_ahead;
		float follow_responsiveness;

		// Path following settings
		float path_smoothing;
		float path_bank_factor;
		float path_bank_speed;

		constexpr Camera(
			float x = 0.0f,
			float y = 0.0f,
			float z = 5.0f,
			float pitch = 0.0f,
			float yaw = 0.0f,
			float roll = 0.0f,
			float fov = Constants::Project::Camera::DefaultFOV(),
			float speed = Constants::Project::Camera::DefaultSpeed(),
			float follow_dist = Constants::Project::Camera::ChaseTrailBehind(),
			float follow_elev = Constants::Project::Camera::ChaseElevation(),
			float follow_ahead = Constants::Project::Camera::ChaseLookAhead(),
			float follow_resp = Constants::Project::Camera::ChaseResponsiveness(),
			float path_smooth = Constants::Project::Camera::PathFollowSmoothing(),
			float path_bank_f = Constants::Project::Camera::PathBankFactor(),
			float path_bank_s = Constants::Project::Camera::PathBankSpeed()
		):
			x(x),
			y(y),
			z(z),
			pitch(pitch),
			yaw(yaw),
			roll(roll),
			fov(fov),
			speed(speed),
			follow_distance(follow_dist),
			follow_elevation(follow_elev),
			follow_look_ahead(follow_ahead),
			follow_responsiveness(follow_resp),
			path_smoothing(path_smooth),
			path_bank_factor(path_bank_f),
			path_bank_speed(path_bank_s) {}

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
	class ITerrainGenerator;

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

		// Start the visualization loop
		void Run();

		// Prepare the visualizer for running. Called automatically by Run(), but can be called
		// manually if you need to ensure all systems are ready before starting.
		// This handles:
		// - Pre-flight checks and validation
		// - Cache warming (terrain chunks, textures)
		// - Invoking registered prepare callbacks
		// Safe to call multiple times (will only prepare once).
		void Prepare();

		// Add a callback to be invoked during Prepare(), after all internal systems
		// are ready but before the main loop starts. Useful for:
		// - Loading additional resources
		// - Setting up initial game state
		// - Pre-spawning entities
		// Callbacks are invoked in the order they were added.
		void AddPrepareCallback(PrepareCallback callback);

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
		void AddChaseTarget(std::shared_ptr<EntityBase> target);
		void CycleChaseTarget();
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
		void                        SetEffectEnabled(VisualEffect effect, bool enabled);
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
			float            intensity = Constants::Class::Shockwaves::DefaultIntensity(),
			float            ring_width = Constants::Class::Shockwaves::DefaultRingWidth(),
			const glm::vec3& color = Constants::Class::Shockwaves::DefaultColor()
		);

		/**
		 * @brief SDF Volume management
		 */
		int  AddSdfSource(const SdfSource& source);
		void UpdateSdfSource(int id, const SdfSource& source);
		void RemoveSdfSource(int id);

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

		void
		ExplodeShape(std::shared_ptr<Shape> shape, float intensity = 1.0f, const glm::vec3& velocity = glm::vec3(0.0f));

		/**
		 * @brief A high-level effect helper that combines mesh explosion, hiding the original shape,
		 * and spawning fire/shockwave effects.
		 *
		 * @param shape The shape to explode and hide
		 * @param direction The primary direction of the explosion and shockwave normal
		 * @param intensity Scaling factor for all combined effects
		 * @param fire_style The style of fire particles to spawn (e.g., Explosion, Glitter)
		 */
		void TriggerComplexExplosion(
			std::shared_ptr<Shape> shape,
			const glm::vec3&       direction,
			float                  intensity = 1.0f,
			FireEffectStyle        fire_style = FireEffectStyle::Explosion
		);

		std::tuple<float, glm::vec3>                 GetTerrainPointProperties(float x, float y) const;
		std::tuple<float, glm::vec3>                 GetTerrainPointPropertiesThreadSafe(float x, float y) const;
		float                                        GetTerrainMaxHeight() const;
		const std::vector<std::shared_ptr<Terrain>>& GetTerrainChunks() const;

		/**
		 * @brief Get the current terrain generator.
		 *
		 * Returns a shared_ptr to ensure safe access even if the terrain generator
		 * is swapped at runtime. The returned pointer remains valid as long as
		 * the caller holds the shared_ptr.
		 *
		 * @return Shared pointer to the terrain generator interface (nullptr if terrain disabled)
		 */
		std::shared_ptr<ITerrainGenerator>       GetTerrain();
		std::shared_ptr<const ITerrainGenerator> GetTerrain() const;

		/**
		 * @brief Create and set a terrain generator.
		 *
		 * Creates a new terrain generator of the specified type with the given
		 * constructor arguments. The Visualizer takes ownership of the generator.
		 *
		 * Note: This will invalidate all existing terrain chunks. The new generator
		 * will begin streaming in chunks based on the current camera position.
		 *
		 * @tparam T Terrain generator type (must derive from ITerrainGenerator)
		 * @tparam Args Constructor argument types
		 * @param args Arguments forwarded to T's constructor
		 * @return Shared pointer to the newly created generator
		 *
		 * Example:
		 * @code
		 * // Create default TerrainGenerator
		 * auto terrain = visualizer.SetTerrainGenerator<TerrainGenerator>();
		 *
		 * // Create with custom seed
		 * auto terrain = visualizer.SetTerrainGenerator<TerrainGenerator>(42);
		 *
		 * // Create custom terrain type
		 * auto terrain = visualizer.SetTerrainGenerator<MyCustomTerrain>(config);
		 * @endcode
		 */
		template <typename T, typename... Args>
			requires std::derived_from<T, ITerrainGenerator>
		std::shared_ptr<T> SetTerrainGenerator(Args&&... args);

		task_thread_pool::task_thread_pool&    GetThreadPool();
		LightManager&                          GetLightManager();
		FireEffectManager*                     GetFireEffectManager();
		DecorManager*                          GetDecorManager();
		PostProcessing::PostProcessingManager& GetPostProcessingManager();
		float                                  GetLastFrameTime() const;

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

		// Internal helper for SetTerrainGenerator template
		void InstallTerrainGenerator(std::shared_ptr<ITerrainGenerator> generator);
	};

	// Template implementation must be in header
	template <typename T, typename... Args>
		requires std::derived_from<T, ITerrainGenerator>
	std::shared_ptr<T> Visualizer::SetTerrainGenerator(Args&&... args) {
		auto generator = std::make_shared<T>(std::forward<Args>(args)...);
		InstallTerrainGenerator(generator);
		return generator;
	}

} // namespace Boidsish