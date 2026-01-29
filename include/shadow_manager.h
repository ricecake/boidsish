#pragma once

#include <array>
#include <memory>
#include <vector>

#include "frustum.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "constants.h"

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
		static constexpr int kMaxShadowLights = Constants::Class::Shadows::MaxLights();
		static constexpr int kMaxCascades = Constants::Class::Shadows::MaxCascades();
		static constexpr int kMaxShadowMaps = Constants::Class::Shadows::MaxShadowMaps();

		/// Shadow map resolution (width and height)
		static constexpr int kShadowMapSize = Constants::Class::Shadows::MapSize();

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
		 * @param map_index Index into the shadow map array
		 * @param light The light to generate shadows for
		 * @param scene_center Center of the scene for shadow frustum calculation
		 * @param scene_radius Radius of the scene for shadow frustum calculation (default 100.0)
		 * @param cascade_index Cascade index if using CSM, -1 otherwise
		 * @param view Camera view matrix for CSM frustum calculation
		 * @param projection Camera projection matrix for CSM frustum calculation
		 */
		void BeginShadowPass(
			int              map_index,
			const Light&     light,
			const glm::vec3& scene_center,
			float            scene_radius = Constants::Class::Shadows::DefaultSceneRadius(),
			int              cascade_index = -1,
			const glm::mat4& view = glm::mat4(1.0f),
			float            fov = Constants::Class::Shadows::DefaultFOV(),
			float            aspect = 1.0f
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
		 * @param map_index Index of the shadow map
		 * @return The view-projection matrix from the light's perspective
		 */
		const glm::mat4& GetLightSpaceMatrix(int map_index) const;

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
		 * @brief Set the cascade split distances.
		 */
		void SetCascadeSplits(const std::array<float, kMaxCascades>& splits) { cascade_splits_ = splits; }

		/**
		 * @brief Get the cascade split distances.
		 */
		const std::array<float, kMaxCascades>& GetCascadeSplits() const { return cascade_splits_; }

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

		/**
		 * @brief Get the world-space frustum for a given shadow map.
		 */
		Frustum GetShadowFrustum(int map_index) const;

	private:
		bool                    initialized_ = false;
		GLuint                  shadow_fbo_ = 0;
		GLuint                  shadow_map_array_ = 0; // 2D texture array for all shadow maps
		GLuint                  shadow_ubo_ = 0;
		std::unique_ptr<Shader> shadow_shader_;

		int                                   active_shadow_count_ = 0;
		std::array<glm::mat4, kMaxShadowMaps> light_space_matrices_;
		// Cascade splits: logarithmic distribution for better near-field detail
		// Near splits are tighter for crisp close shadows
		// Far cascade (3) acts as catchall extending to very distant terrain
		std::array<float, kMaxCascades>       cascade_splits_ = {20.0f, 50.0f, 150.0f, 700.0f};

		// Previous viewport for restoration
		GLint prev_viewport_[4];

		std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
	};

} // namespace Boidsish
