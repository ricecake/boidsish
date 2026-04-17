#include "volumetric_lighting_manager.h"
#include <iostream>
#include <shader.h>
#include "logger.h"

namespace Boidsish {

	VolumetricLightingManager::VolumetricLightingManager() {}

	VolumetricLightingManager::~VolumetricLightingManager() {
		for (auto& cascade : cascades_) {
			if (cascade.texture) {
				glDeleteTextures(1, &cascade.texture);
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

			cascades_[i].near_dist = (i == 0) ? near_plane : cascade_fars[i-1];
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
	}

	void VolumetricLightingManager::Update(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos, const glm::vec3& camera_front, float /*delta_time*/) {
		if (!initialized_) return;

		frame_counter_++;

		// Re-initialize grid if projection changed or camera moved/rotated
		bool projection_changed = (projection != last_projection_);
		bool camera_moved = (glm::distance(camera_pos, last_camera_pos_) > 0.1f);
		bool camera_rotated = (glm::dot(camera_front, last_camera_front_) < 0.999f);

		// Determine which cascades to update based on frequency
		// Cascade 0: Every frame
		// Cascade 1: Every 2 frames
		// Cascade 2: Every 4 frames
		bool update_c0 = true;
		bool update_c1 = (frame_counter_ % 2 == 0);
		bool update_c2 = (frame_counter_ % 4 == 0);

		if (projection_changed || camera_moved || camera_rotated) {
			last_projection_ = projection;
			last_camera_pos_ = camera_pos;
			last_camera_front_ = camera_front;

			grid_init_shader_->use();

			VolumetricLightingUbo ubo_data;
			ubo_data.inv_view_proj = glm::inverse(projection * view);
			ubo_data.grid_size = glm::vec4(grid_x_, grid_y_, grid_z_, num_cascades_);

			float near_plane = 0.1f;
			float far_plane = Constants::Project::Camera::DefaultFarPlane();
			ubo_data.clip_params = glm::vec4(near_plane, far_plane, 0.0f, 0.0f);

			ubo_data.cascade_fars = glm::vec4(0.0f);
			for (int i = 0; i < num_cascades_; ++i) {
				ubo_data.cascade_fars[i] = cascades_[i].far_dist;
			}

			glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VolumetricLightingUbo), &ubo_data);

			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), lighting_ubo_, 0, sizeof(VolumetricLightingUbo));
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricFroxelData(), froxel_data_ssbo_);

			// Dispatch initialization for cascades that need update
			// For Phase 1, we update all if anything changed, but we can respect flags
			if (update_c0) {
				// Update cascade 0
				grid_init_shader_->setInt("u_cascadeIdx", 0);
				glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
			}
			if (update_c1 && num_cascades_ > 1) {
				grid_init_shader_->setInt("u_cascadeIdx", 1);
				glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
			}
			if (update_c2 && num_cascades_ > 2) {
				grid_init_shader_->setInt("u_cascadeIdx", 2);
				glDispatchCompute((grid_x_ + 7) / 8, (grid_y_ + 7) / 8, (grid_z_ + 7) / 8);
			}

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

} // namespace Boidsish
