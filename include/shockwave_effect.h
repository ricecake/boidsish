#pragma once

#include <memory>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	/**
	 * @brief Represents a single expanding shockwave ring effect.
	 *
	 * Shockwaves are spawned at explosion points and expand outward over time,
	 * creating visual distortion effects on geometry and in screen-space.
	 */
	struct Shockwave {
		glm::vec3 center;         ///< World-space origin of the shockwave
		glm::vec3 normal;         ///< World-space normal of the shockwave plane
		float     max_radius;     ///< Maximum radius the wave will expand to
		float     current_radius; ///< Current expansion radius
		float     duration;       ///< Total lifetime of the shockwave in seconds
		float     elapsed_time;   ///< Time since shockwave was created
		float     intensity;      ///< Distortion intensity (0.0 to 1.0)
		float     ring_width;     ///< Width of the distortion ring
		glm::vec3 color;          ///< Color tint of the shockwave (for glow effects)

		/**
		 * @brief Calculate the normalized age of the shockwave (0 to 1)
		 */
		float GetNormalizedAge() const { return duration > 0.0f ? elapsed_time / duration : 1.0f; }

		/**
		 * @brief Check if the shockwave has completed its lifetime
		 */
		bool IsExpired() const { return elapsed_time >= duration; }

		/**
		 * @brief Get the current intensity accounting for age fade-out
		 */
		float GetEffectiveIntensity() const {
			float age = GetNormalizedAge();
			// Smooth fade out using inverse square for dramatic falloff
			return intensity * (1.0f - age * age);
		}
	};

	/**
	 * @brief GPU-aligned shockwave data for shader communication
	 *
	 * This structure is mirrored in the shockwave shaders and must maintain
	 * std140 layout compatibility.
	 */
	struct ShockwaveGPUData {
		glm::vec4 center_radius; ///< xyz = center, w = current_radius
		glm::vec4 normal_unused; ///< xyz = normal, w = unused
		glm::vec4 params;        ///< x = intensity, y = ring_width, z = max_radius, w = normalized_age
		glm::vec4 color_unused;  ///< xyz = color, w = unused
	};

	static_assert(sizeof(ShockwaveGPUData) == 64, "ShockwaveGPUData must be 64 bytes for std140 alignment");

	/**
	 * @brief Manages active shockwave effects and their GPU rendering.
	 *
	 * The ShockwaveManager handles:
	 * - Creating and tracking active shockwaves
	 * - Updating shockwave physics (expansion over time)
	 * - Providing data to shaders for screen-space distortion
	 * - Providing data for vertex displacement (terrain/entities)
	 */
	class ShockwaveManager {
	public:
		/// Maximum number of simultaneous shockwaves (limited by UBO size)
		static constexpr int kMaxShockwaves = 16;

		ShockwaveManager();
		~ShockwaveManager();

		/**
		 * @brief Add a new shockwave effect at the given position.
		 *
		 * @param center World-space origin of the explosion
		 * @param max_radius Maximum expansion radius
		 * @param duration Time in seconds for the wave to reach max_radius
		 * @param intensity Distortion strength (0.0 to 1.0, default 0.5)
		 * @param ring_width Width of the distortion ring in world units
		 * @param color Color tint for the shockwave glow
		 * @return true if the shockwave was added, false if at capacity
		 */
		bool AddShockwave(
			const glm::vec3& center,
			const glm::vec3& normal,
			float            max_radius,
			float            duration,
			float            intensity = 0.5f,
			float            ring_width = 3.0f,
			const glm::vec3& color = glm::vec3(1.0f, 0.6f, 0.2f)
		);

		/**
		 * @brief Update all active shockwaves.
		 *
		 * Advances shockwave timers and removes expired effects.
		 *
		 * @param delta_time Time elapsed since last update in seconds
		 */
		void Update(float delta_time);

		/**
		 * @brief Apply screen-space distortion effect.
		 *
		 * This renders the shockwave distortion as a post-processing effect
		 * using screen-space projection of shockwave positions.
		 *
		 * @param source_texture The scene color texture to distort
		 * @param view_matrix The current view matrix
		 * @param proj_matrix The current projection matrix
		 * @param camera_pos The camera world position
		 * @param quad_vao VAO for full-screen quad rendering
		 */
		void ApplyScreenSpaceEffect(
			GLuint           source_texture,
			const glm::mat4& view_matrix,
			const glm::mat4& proj_matrix,
			const glm::vec3& camera_pos,
			GLuint           quad_vao
		);

		/**
		 * @brief Upload shockwave data to a UBO for vertex shader access.
		 *
		 * Call this before rendering terrain or entities that should be
		 * displaced by shockwaves.
		 */
		void UpdateShaderData();

		/**
		 * @brief Bind the shockwave UBO to a binding point.
		 *
		 * @param binding_point The UBO binding point to use
		 */
		void BindUBO(GLuint binding_point) const;

		/**
		 * @brief Get the number of currently active shockwaves.
		 */
		int GetActiveCount() const { return static_cast<int>(shockwaves_.size()); }

		/**
		 * @brief Check if any shockwaves are currently active.
		 */
		bool HasActiveShockwaves() const { return !shockwaves_.empty(); }

		/**
		 * @brief Get read-only access to active shockwaves.
		 */
		const std::vector<Shockwave>& GetShockwaves() const { return shockwaves_; }

		/**
		 * @brief Clear all active shockwaves immediately.
		 */
		void Clear() { shockwaves_.clear(); }

		/**
		 * @brief Initialize GPU resources (call once after OpenGL context creation)
		 */
		void Initialize(int screen_width, int screen_height);

		/**
		 * @brief Handle screen resize
		 */
		void Resize(int width, int height);

		/**
		 * @brief Set the global intensity multiplier for all shockwaves
		 */
		void SetGlobalIntensity(float intensity) { global_intensity_ = intensity; }

		/**
		 * @brief Get the global intensity multiplier
		 */
		float GetGlobalIntensity() const { return global_intensity_; }

	private:
		void EnsureInitialized();

		std::vector<Shockwave>  shockwaves_;
		std::unique_ptr<Shader> shader_;
		GLuint                  ubo_{0};
		bool                    initialized_{false};
		int                     screen_width_{0};
		int                     screen_height_{0};
		float                   global_intensity_{1.0f};

		// Intermediate FBO for effect rendering
		GLuint fbo_{0};
		GLuint output_texture_{0};
	};

} // namespace Boidsish
