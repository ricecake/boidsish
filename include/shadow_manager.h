#pragma once

#include <memory>
#include <vector>
#include <array>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	struct Light;

	/**
	 * @brief Manages shadow map generation and shadow data for the lighting system.
	 *
	 * The ShadowManager handles:
	 * - Creating and managing shadow map depth textures
	 * - Computing light-space matrices for shadow projection
	 * - Providing shadow data to shaders via UBO
	 *
	 * Shadow maps use a texture array to support multiple shadow-casting lights.
	 */
	class ShadowManager {
	public:
		/// Maximum number of shadow-casting lights supported
		static constexpr int kMaxShadowLights = 4;

		/// Shadow map resolution (width and height)
		static constexpr int kShadowMapSize = 2048;

		ShadowManager();
		~ShadowManager();

		// Non-copyable
		ShadowManager(const ShadowManager&) = delete;
		ShadowManager& operator=(const ShadowManager&) = delete;

		/**
		 * @brief Initialize OpenGL resources for shadow mapping.
		 *
		 * Creates the shadow map FBO, depth texture array, and shadow shader.
		 * Call once after OpenGL context creation.
		 */
		void Initialize();

		/**
		 * @brief Begin rendering to a shadow map for a specific light.
		 *
		 * Sets up the FBO and viewport for shadow map rendering.
		 * After calling this, render your scene geometry using the shadow shader.
		 *
		 * @param light_index Index of the shadow-casting light (0 to kMaxShadowLights-1)
		 * @param light The light to generate shadows for
		 * @param scene_center Center of the scene for shadow frustum calculation
		 * @param scene_radius Radius of the scene for shadow frustum calculation (default 100.0)
		 */
		void BeginShadowPass(
			int              light_index,
			const Light&     light,
			const glm::vec3& scene_center,
			float            scene_radius = 100.0f
		);

		/**
		 * @brief End the shadow pass and restore default framebuffer.
		 */
		void EndShadowPass();

		/**
		 * @brief Get the depth shader for shadow map rendering.
		 *
		 * This shader only writes depth values (no fragment output).
		 */
		Shader& GetShadowShader() const { return *shadow_shader_; }

		/**
		 * @brief Get the light-space matrix for a shadow-casting light.
		 *
		 * @param light_index Index of the shadow-casting light
		 * @return The view-projection matrix from the light's perspective
		 */
		const glm::mat4& GetLightSpaceMatrix(int light_index) const;

		/**
		 * @brief Bind shadow maps and UBO for use in the main render pass.
		 *
		 * @param shader The shader to set up shadow samplers for
		 * @param texture_unit The texture unit to bind the shadow map array to
		 */
		void BindForRendering(Shader& shader, int texture_unit = 4);

		/**
		 * @brief Update the shadow UBO with current light-space matrices.
		 *
		 * Call this after all shadow passes are complete, before the main render.
		 */
		void UpdateShadowUBO(const std::vector<Light*>& shadow_lights);

		/**
		 * @brief Get the shadow map texture array ID.
		 */
		GLuint GetShadowMapArray() const { return shadow_map_array_; }

		/**
		 * @brief Check if shadow mapping is enabled and initialized.
		 */
		bool IsInitialized() const { return initialized_; }

		/**
		 * @brief Get the number of active shadow maps.
		 */
		int GetActiveShadowCount() const { return active_shadow_count_; }

	private:
		bool                    initialized_ = false;
		GLuint                  shadow_fbo_ = 0;
		GLuint                  shadow_map_array_ = 0; // 2D texture array for all shadow maps
		GLuint                  shadow_ubo_ = 0;
		std::unique_ptr<Shader> shadow_shader_;

		int                                     active_shadow_count_ = 0;
		std::array<glm::mat4, kMaxShadowLights> light_space_matrices_;

		// Previous viewport for restoration
		GLint prev_viewport_[4];
	};

} // namespace Boidsish
