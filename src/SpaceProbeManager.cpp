#include "SpaceProbeManager.h"

#include <iostream>
#include <array>
#include <glm/gtc/matrix_transform.hpp>

#include "profiler.h"
#include "shader.h"
#include "constants.h"
#include "atmosphere_manager.h"
#include "terrain_render_manager.h"
#include "shadow_manager.h"
#include "light_manager.h"
#include "render_state.h"

namespace Boidsish {

	SpaceProbeManager::SpaceProbeManager() : camera_follow_mode(false) {}

	SpaceProbeManager::~SpaceProbeManager() {
		if (probe_ssbo_ != 0) {
			glDeleteBuffers(1, &probe_ssbo_);
		}
	}

	void SpaceProbeManager::Initialize() {
		if (initialized_) return;

		collect_shader_ = std::make_unique<ComputeShader>("shaders/effects/space_probe_collect.comp");
		render_shader_ = std::make_unique<Shader>("shaders/effects/space_probe.vert", "shaders/effects/space_probe.frag");

		glGenBuffers(1, &probe_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SpaceProbeData), &cached_data_, GL_DYNAMIC_COPY);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		initialized_ = true;
	}

	void SpaceProbeManager::Update(
		float deltaTime,
		const GlobalRenderState& render_state,
		AtmosphereManager* atmosphere_manager,
		TerrainRenderManager* terrain_render_manager,
		ShadowManager* shadow_manager,
		LightManager* light_manager
	) {
		PROJECT_PROFILE_SCOPE("SpaceProbeManager::Update");
		(void)deltaTime;

		if (camera_follow_mode) {
			is_enabled = true;
			continuous_update = true;

			// Extract camera directions from the view matrix
			glm::vec3 cam_up(render_state.view[0][1], render_state.view[1][1], render_state.view[2][1]);
			glm::vec3 cam_front(-render_state.view[0][2], -render_state.view[1][2], -render_state.view[2][2]);

			cam_up = glm::length(cam_up) > 0.001f ? glm::normalize(cam_up) : glm::vec3(0.0f, 1.0f, 0.0f);
			cam_front = glm::length(cam_front) > 0.001f ? glm::normalize(cam_front) : glm::vec3(0.0f, 0.0f, -1.0f);

			float D = 15.0f;
			float tan_half_fov = 0.57735f; // default fallback for 60 degrees FOV
			if (render_state.projection[1][1] != 0.0f) {
				tan_half_fov = 1.0f / render_state.projection[1][1];
			}
			float half_height = D * tan_half_fov;
			float H = 0.667f * half_height;

			position = render_state.camera_pos + cam_front * D - cam_up * H;
		}

		if (!initialized_ || !is_enabled) return;
		if (!continuous_update && !refresh_requested) return;
		if (!atmosphere_manager || !terrain_render_manager || !shadow_manager || !light_manager) return;

		collect_shader_->use();
		collect_shader_->setVec3("u_probePosition", position);

		// Bind textures for atmospheric lookup and fallback
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereSkyView());
		glBindTexture(GL_TEXTURE_2D, atmosphere_manager->GetSkyViewLUT());
		collect_shader_->setInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
		glBindTexture(GL_TEXTURE_3D, atmosphere_manager->GetAerialPerspectiveLUT());
		collect_shader_->setInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
		glBindTexture(GL_TEXTURE_2D, atmosphere_manager->GetTransmittanceLUT());
		collect_shader_->setInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());

		// Bind terrain textures and UBOs
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, terrain_render_manager->GetBiomeTexture());
		collect_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainChunkGrid());
		glBindTexture(GL_TEXTURE_2D, terrain_render_manager->GetChunkGridTexture());
		collect_shader_->setInt("u_chunkGrid", Constants::TextureUnit::TerrainChunkGrid());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, terrain_render_manager->GetHeightmapTexture());
		collect_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainShadowMap());
		glBindTexture(GL_TEXTURE_2D, terrain_render_manager->GetTerrainShadowMapTexture());
		collect_shader_->setInt("u_terrainShadowMap", Constants::TextureUnit::TerrainShadowMap());

		// Bind Shadows UBO and ShadowMaps texture array (needed for shadow mapping!)
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), shadow_manager->GetShadowUbo());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMaps());
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_manager->GetShadowMapArray());
		collect_shader_->setInt("shadowMaps", Constants::TextureUnit::ShadowMaps());

		// Set lightShadowIndices uniform array explicitly on the shader
		std::array<int, 10> shadow_indices;
		shadow_indices.fill(-1);
		const auto& all_lights = light_manager->GetLights();
		for (size_t j = 0; j < all_lights.size() && j < 10; ++j) {
			shadow_indices[j] = all_lights[j].shadow_map_index;
		}
		collect_shader_->setIntArray("lightShadowIndices", shadow_indices.data(), 10);

		// Bind standard UBO ranges using helper methods
		render_state.BindLighting(Constants::UboBinding::Lighting());
		render_state.BindTemporal(Constants::UboBinding::TemporalData());
		render_state.BindVisualEffects(Constants::UboBinding::VisualEffects());
		render_state.BindFrustum(Constants::UboBinding::FrustumData());

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_render_manager->GetTerrainDataUbo());
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), terrain_render_manager->GetBiomeUbo());

		// Bind the SSBO
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::SpaceProbe(), probe_ssbo_);

		// Dispatch compute shader (only 1 thread needed as it calculates properties for a single point)
		collect_shader_->dispatch(1, 1, 1);

		// Synchronize buffer access before reading back to CPU
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		// Read back results into CPU cache
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_ssbo_);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(SpaceProbeData), &cached_data_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Clear refresh flag
		refresh_requested = false;
	}

	void SpaceProbeManager::Render(
		const GlobalRenderState& render_state,
		GLuint depthTexture,
		GLuint quadVAO,
		ShadowManager* shadow_manager,
		LightManager* light_manager,
		AtmosphereManager* atmosphere_manager,
		TerrainRenderManager* terrain_render_manager
	) {
		PROJECT_PROFILE_SCOPE("SpaceProbeManager::Render");
		if (!initialized_ || !is_enabled || !render_sphere || !shadow_manager || !light_manager || !atmosphere_manager || !terrain_render_manager) return;

		// Force standard depth test and write state explicitly to prevent transparent pass leaks
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);

		render_shader_->use();

		// Transformation matrices and camera position
		render_shader_->setVec3("cameraPos", render_state.camera_pos);
		render_shader_->setMat4("invProjection", glm::inverse(render_state.projection));
		render_shader_->setMat4("invView", glm::inverse(render_state.view));
		render_shader_->setMat4("projection", render_state.projection);
		render_shader_->setMat4("view", render_state.view);

		// Custom sphere properties and toggles
		render_shader_->setVec3("u_probePosition", position);
		render_shader_->setFloat("u_probeRadius", sphere_radius);
		render_shader_->setVec3("u_probeColor", sphere_color);
		render_shader_->setFloat("u_probeMetallic", metallic);
		render_shader_->setFloat("u_probeRoughness", roughness);
		render_shader_->setBool("u_useSHVisualizer", use_sh_visualizer);

		// Bind scene depth texture for proper occlusion
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		render_shader_->setInt("depthTexture", 0);

		// Bind Shadows UBO and ShadowMaps texture array (needed for shadow mapping!)
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), shadow_manager->GetShadowUbo());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMaps());
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_manager->GetShadowMapArray());
		render_shader_->setInt("shadowMaps", Constants::TextureUnit::ShadowMaps());

		// Set lightShadowIndices uniform array explicitly on the shader
		std::array<int, 10> shadow_indices;
		shadow_indices.fill(-1);
		const auto& all_lights = light_manager->GetLights();
		for (size_t j = 0; j < all_lights.size() && j < 10; ++j) {
			shadow_indices[j] = all_lights[j].shadow_map_index;
		}
		render_shader_->setIntArray("lightShadowIndices", shadow_indices.data(), 10);

		// Bind textures for atmospheric lookup
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereSkyView());
		glBindTexture(GL_TEXTURE_2D, atmosphere_manager->GetSkyViewLUT());
		render_shader_->setInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
		glBindTexture(GL_TEXTURE_3D, atmosphere_manager->GetAerialPerspectiveLUT());
		render_shader_->setInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
		glBindTexture(GL_TEXTURE_2D, atmosphere_manager->GetTransmittanceLUT());
		render_shader_->setInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());

		// Bind terrain textures and UBOs
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, terrain_render_manager->GetBiomeTexture());
		render_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainChunkGrid());
		glBindTexture(GL_TEXTURE_2D, terrain_render_manager->GetChunkGridTexture());
		render_shader_->setInt("u_chunkGrid", Constants::TextureUnit::TerrainChunkGrid());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, terrain_render_manager->GetHeightmapTexture());
		render_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainShadowMap());
		glBindTexture(GL_TEXTURE_2D, terrain_render_manager->GetTerrainShadowMapTexture());
		render_shader_->setInt("u_terrainShadowMap", Constants::TextureUnit::TerrainShadowMap());

		// Bind standard UBO ranges using helper methods
		render_state.BindLighting(Constants::UboBinding::Lighting());
		render_state.BindTemporal(Constants::UboBinding::TemporalData());
		render_state.BindVisualEffects(Constants::UboBinding::VisualEffects());
		render_state.BindFrustum(Constants::UboBinding::FrustumData());

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_render_manager->GetTerrainDataUbo());
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), terrain_render_manager->GetBiomeUbo());

		// Bind the space probe SSBO so the sphere can sample computed SH coefficients
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::SpaceProbe(), probe_ssbo_);

		// Render quad to evaluate sphere SDF and shade it in screenspace
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

} // namespace Boidsish
