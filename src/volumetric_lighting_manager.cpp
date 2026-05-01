#include "volumetric_lighting_manager.h"
#include "service_locator.h"
#include "shader.h"
#include "constants.h"
#include "logger.h"
#include "profiler.h"
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	VolumetricLightingManager::VolumetricLightingManager(ServiceLocator& /*loc*/) {}

	VolumetricLightingManager::~VolumetricLightingManager() {
		if (volumetric_texture_) glDeleteTextures(1, &volumetric_texture_);
		if (accumulation_ssbo_) glDeleteBuffers(1, &accumulation_ssbo_);
		if (params_ubo_) glDeleteBuffers(1, &params_ubo_);
	}

	void VolumetricLightingManager::Initialize() {
		CreateResources();
		CreateShaders();
	}

	void VolumetricLightingManager::CreateResources() {
		// Final 3D Texture
		glGenTextures(1, &volumetric_texture_);
		glBindTexture(GL_TEXTURE_3D, volumetric_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, grid_res_.x, grid_res_.y, grid_res_.z, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Accumulation SSBO (3 uints per froxel for R, G, B)
		size_t total_elements = grid_res_.x * grid_res_.y * grid_res_.z * 3;
		glGenBuffers(1, &accumulation_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, accumulation_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, total_elements * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		// Params UBO
		glGenBuffers(1, &params_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, params_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(VolumetricLightingParams), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void VolumetricLightingManager::CreateShaders() {
		clear_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_clear.comp");
		inject_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_particle_injection.comp");
		resolve_shader_ = std::make_unique<ComputeShader>("shaders/volumetric_particle_resolve.comp");

		if (!clear_shader_->isValid() || !inject_shader_->isValid() || !resolve_shader_->isValid()) {
			logger::ERROR("Failed to compile volumetric lighting shaders");
		}
	}

	void VolumetricLightingManager::Update(
		const glm::mat4& view_proj,
		const glm::mat4& inv_view_proj,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cam_pos,
		const glm::vec3& cam_dir,
		float            near_p,
		float            far_p,
		float            simulation_time
	) {
		params_.view_projection = view_proj;
		params_.inv_view_projection = inv_view_proj;
		params_.view = view;
		params_.projection = projection;
		params_.camera_pos_near = glm::vec4(cam_pos, near_p);
		params_.camera_dir_far = glm::vec4(cam_dir, far_p);
		params_.grid_res_pad = glm::ivec4(grid_res_, 0);
		simulation_time_ = simulation_time;

		glBindBuffer(GL_UNIFORM_BUFFER, params_ubo_);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VolumetricLightingParams), &params_);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void VolumetricLightingManager::Clear() {
		PROJECT_PROFILE_SCOPE("VolumetricLighting::Clear");
		clear_shader_->use();
		int total_elements = grid_res_.x * grid_res_.y * grid_res_.z * 3;
		clear_shader_->setInt("u_total_elements", total_elements);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricLightAccumulation(), accumulation_ssbo_);
		glDispatchCompute((total_elements + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void VolumetricLightingManager::InjectParticles(
		GLuint particle_buffer,
		int max_particles,
		GLuint live_indices_buffer,
		GLuint behavior_command_buffer
	) {
		PROJECT_PROFILE_SCOPE("VolumetricLighting::InjectParticles");
		inject_shader_->use();
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), params_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricLightAccumulation(), accumulation_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::LiveParticleIndices(), live_indices_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::BehaviorDrawCommand(), behavior_command_buffer);

		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, behavior_command_buffer);
		glDispatchComputeIndirect(0);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void VolumetricLightingManager::Resolve() {
		PROJECT_PROFILE_SCOPE("VolumetricLighting::Resolve");
		resolve_shader_->use();
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), params_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VolumetricLightAccumulation(), accumulation_ssbo_);
		glBindImageTexture(Constants::TextureUnit::VolumetricLightTexture(), volumetric_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		glDispatchCompute((grid_res_.x + 7) / 8, (grid_res_.y + 7) / 8, grid_res_.z);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	void VolumetricLightingManager::BindForSampling(GLuint unit) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_3D, volumetric_texture_);
	}

} // namespace Boidsish
