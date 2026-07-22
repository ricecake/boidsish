#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <GL/glew.h>

class ComputeShader;
class Shader;

namespace Boidsish {

	struct GlobalRenderState;
	class AtmosphereManager;
	class TerrainRenderManager;
	class ShadowManager;

	struct SpaceProbeData {
		float     shadow_level = 1.0f;
		float     shadow_from_terrain = 1.0f;
		float     cloud_shadow = 1.0f;
		float     ambient_occlusion = 1.0f;
		glm::vec4 light_directional = glm::vec4(0.0f);
		glm::vec4 light_other = glm::vec4(0.0f);
		glm::vec4 light_ambient = glm::vec4(0.0f);
		glm::vec4 sh_coeffs[9];
	};

	class SpaceProbeManager {
	public:
		SpaceProbeManager();
		~SpaceProbeManager();

		void Initialize();
		void Update(
			float deltaTime,
			const GlobalRenderState& render_state,
			AtmosphereManager* atmosphere_manager,
			TerrainRenderManager* terrain_render_manager,
			ShadowManager* shadow_manager
		);
		void Render(
			const GlobalRenderState& render_state,
			GLuint depthTexture,
			GLuint quadVAO,
			ShadowManager* shadow_manager
		);

		// Properties and Settings
		bool      is_enabled = false;
		glm::vec3 position = glm::vec3(0.0f, 10.0f, 0.0f);
		bool      continuous_update = true;
		bool      render_sphere = true;
		float     sphere_radius = 2.0f;
		glm::vec3 sphere_color = glm::vec3(1.0f);
		float     metallic = 0.0f;
		float     roughness = 0.5f;
		bool      use_sh_visualizer = true;
		bool      refresh_requested = false;

		// Get readback data
		const SpaceProbeData& GetData() const { return cached_data_; }

	private:
		std::unique_ptr<ComputeShader> collect_shader_;
		std::unique_ptr<Shader>        render_shader_;
		GLuint                         probe_ssbo_ = 0;
		SpaceProbeData                 cached_data_;
		bool                           initialized_ = false;
	};

} // namespace Boidsish
