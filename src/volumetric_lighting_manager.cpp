#include "volumetric_lighting_manager.h"
#include <iostream>
#include <shader.h>
#include "logger.h"
#include "weather_manager.h"
#include "light_manager.h"
#include "shadow_manager.h"
#include "terrain_render_manager.h"
#include "atmosphere_manager.h"
#include "weather_constants.h"
#include <GLFW/glfw3.h>

namespace Boidsish {

	VolumetricLightingManager::VolumetricLightingManager() {}

	VolumetricLightingManager::~VolumetricLightingManager() {
		for (auto& cascade : cascades_) {
			if (cascade.texture) {
				glDeleteTextures(1, &cascade.texture);
			}
			if (cascade.density_texture) {
				glDeleteTextures(1, &cascade.density_texture);
			}
		}
		if (froxel_data_ssbo_) {
			glDeleteBuffers(1, &froxel_data_ssbo_);
		}
		if (lighting_ubo_) {
			glDeleteBuffers(1, &lighting_ubo_);
		}
	}

	void VolumetricLightingManager::Initialize() {
		if (initialized_) return;

		CreateTextures();
		CreateBuffers();
		CreateShaders();

		initialized_ = true;
		logger::LOG("VolumetricLightingManager initialized.");
	}

	void VolumetricLightingManager::CreateTextures() {
		cascades_.resize(num_cascades_);

		// Define cascade distances (exponentially spaced for now)
		float near_plane = 0.1f;
		float cascade_fars[] = { 20.0f, 100.0f, 500.0f };

		for (int i = 0; i < num_cascades_; ++i) {
			glGenTextures(1, &cascades_[i].texture);
			glBindTexture(GL_TEXTURE_3D, cascades_[i].texture);

			// Using RGBA16F for volumetric data (scattering.xyz, extinction.w)
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, grid_x_, grid_y_, grid_z_, 0, GL_RGBA, GL_FLOAT, nullptr);

			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &cascades_[i].density_texture);
			glBindTexture(GL_TEXTURE_3D, cascades_[i].density_texture);
			// Using RGBA16F for accumulated density/color (scattering_rgb, extinction_a)
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, grid_x_, grid_y_, grid_z_, 0, GL_RGBA, GL_FLOAT, nullptr);

			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			cascades_[i].near_dist = (i == 0) ? near_plane : cascade_fars[i - 1];
			cascades_[i].far_dist = cascade_fars[i];
		}
		glBindTexture(GL_TEXTURE_3D, 0);
	}

	void VolumetricLightingManager::CreateBuffers() {
		// SSBO for Froxel AABBs (2 * vec4 per froxel: min and max)
		size_t num_froxels = grid_x_ * grid_y_ * grid_z_ * num_cascades_;
		glGenBuffers(1, &froxel_data_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, froxel_data_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, num_froxels * 2 * sizeof(glm::vec4), nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// UBO for volumetric lighting parameters
		glGenBuffers(1, &lighting_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(VolumetricLightingUbo), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void VolumetricLightingManager::CreateShaders() {
		grid_init_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_grid_init.comp");
		density_init_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_density_init.comp");
		voxelize_particles_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_voxelize_particles.comp");
		inject_clouds_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_clouds_injection.comp");
		lighting_injection_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_lighting_injection.comp");
	}

	void VolumetricLightingManager::Update(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos, const glm::vec3& camera_front, float /*delta_time*/, const CurrentWeather& weather, float world_scale, LightManager& light_manager, ShadowManager* shadow_manager, TerrainRenderManager* terrain_render_manager, AtmosphereManager* atmosphere_manager) {
		if (!initialized_) return;

		frame_counter_++;

		// Update UBO data
		VolumetricLightingUbo ubo_data;
		ubo_data.view = view;
		ubo_data.projection = projection;
		ubo_data.inv_view_proj = glm::inverse(projection * view);
		ubo_data.grid_size = glm::vec4(grid_x_, grid_y_, grid_z_, num_cascades_);

		float near_plane = 0.1f;
		float far_plane = Constants::Project::Camera::DefaultFarPlane() * world_scale;
		float log_base = 0.0f; // Could be computed if needed
		ubo_data.clip_params = glm::vec4(near_plane, far_plane, log_base, world_scale);

		ubo_data.cascade_fars = glm::vec4(0.0f);
		for (int i = 0; i < num_cascades_; ++i) {
			ubo_data.cascade_fars[i] = cascades_[i].far_dist * world_scale;
		}

		ubo_data.haze_params = glm::vec4(weather.haze_density, weather.haze_height, WeatherConstants::MieAnisotropy, 0.0f);
		ubo_data.haze_color = glm::vec4(0.6f, 0.7f, 0.8f, 1.0f); // Fallback color
		ubo_data.cloud_params = glm::vec4(weather.cloud_altitude, weather.cloud_thickness, weather.cloud_density, weather.cloud_coverage);
		ubo_data.cloud_params2 = glm::vec4(0.0f, (float)glfwGetTime(), 0.0f, 0.0f);

		glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VolumetricLightingUbo), &ubo_data);
		glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), lighting_ubo_, 0, sizeof(VolumetricLightingUbo));

		// Re-initialize grid if projection changed or camera moved/rotated
		bool projection_changed = (projection != last_projection_);
		bool camera_moved = (glm::distance(camera_pos, last_camera_pos_) > 0.1f);
		bool camera_rotated = (glm::dot(camera_front, last_camera_front_) < 0.999f);

		if (projection_changed || camera_moved || camera_rotated) {
			last_projection_ = projection;
			last_camera_pos_ = camera_pos;
			last_camera_front_ = camera_front;

			grid_init_shader_->use();
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricFroxelData(), froxel_data_ssbo_);

			for (int i = 0; i < num_cascades_; ++i) {
				grid_init_shader_->setInt("u_cascadeIdx", i);
				glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// Phase 2: Density and Media Voxelization

		// 1. Initialize density grid with fog
		density_init_shader_->use();
		for (int i = 0; i < num_cascades_; ++i) {
			density_init_shader_->setInt("u_cascadeIdx", i);
			glBindImageTexture(0, cascades_[i].density_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// 2. Voxelize particles (local cascades 0 and 1)
		voxelize_particles_shader_->use();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), 16); // [[PARTICLE_BUFFER_BINDING]]
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleDrawCommand(), 28);   // [[PARTICLE_DRAW_COMMAND_BINDING]]
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricFroxelData(), froxel_data_ssbo_);

		for (int i = 0; i < std::min(num_cascades_, 2); ++i) {
			voxelize_particles_shader_->setInt("u_cascadeIdx", i);
			glBindImageTexture(0, cascades_[i].density_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
			// Dispatch depends on particle count, but for scattering we can iterate particles or cells.
			// Task says: "dispatch a compute pass that scatters particle density and color into the grid"
			// So we dispatch per particle.
			glDispatchCompute((Constants::Class::Particles::MaxParticles() + 63) / 64, 1, 1);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// 3. Inject cloud density (outer cascades 1 and 2)
		inject_clouds_shader_->use();
		for (int i = 1; i < num_cascades_; ++i) {
			inject_clouds_shader_->setInt("u_cascadeIdx", i);
			glBindImageTexture(0, cascades_[i].density_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
			glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Phase 3: Lighting and Shadow Injection
		if (lighting_injection_shader_ && lighting_injection_shader_->isValid()) {
			lighting_injection_shader_->use();

			// Bind UBOs
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), lighting_ubo_);

			if (shadow_manager) {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), shadow_manager->GetShadowUbo());
				glActiveTexture(GL_TEXTURE10);
				glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_manager->GetShadowMapArray());
				lighting_injection_shader_->setInt("shadowMaps", 10);
			}

			if (terrain_render_manager) {
				terrain_render_manager->BindTerrainData(*lighting_injection_shader_);
			}

			if (atmosphere_manager) {
				atmosphere_manager->BindToShader(*lighting_injection_shader_);
			}

			// Bind SSBOs
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricFroxelData(), froxel_data_ssbo_);

			// Set light shadow indices
			std::array<int, 10> shadow_indices;
			shadow_indices.fill(-1);
			const auto& all_lights = light_manager.GetLights();
			for (size_t j = 0; j < all_lights.size() && j < 10; ++j) {
				shadow_indices[j] = all_lights[j].shadow_map_index;
			}
			lighting_injection_shader_->setIntArray("lightShadowIndices", shadow_indices.data(), 10);

			for (int i = 0; i < num_cascades_; ++i) {
				lighting_injection_shader_->setInt("u_cascadeIdx", i);
				// Read from accumulated density
				glBindImageTexture(0, cascades_[i].density_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
				// Write to final scattering/extinction texture
				glBindImageTexture(1, cascades_[i].texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

				glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
	}

} // namespace Boidsish
