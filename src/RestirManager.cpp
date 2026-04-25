#include "RestirManager.h"
#include "service_locator.h"
#include "light_manager.h"
#include "fire_effect_manager.h"
#include "constants.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>

namespace Boidsish {

	RestirManager::RestirManager(ServiceLocator& loc) : loc_(loc) {}

	RestirManager::~RestirManager() {
		if (reservoir_buffers_[0]) glDeleteBuffers(2, reservoir_buffers_);
		if (gi_reservoir_buffers_[0]) glDeleteBuffers(2, gi_reservoir_buffers_);
	}

	void RestirManager::Initialize() {
		di_sampling_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_sampling.comp");
		di_temporal_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_temporal.comp");
		di_spatial_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_spatial.comp");
		gi_trace_shader_ = std::make_unique<ComputeShader>("shaders/restir/gi_trace.comp");
		gi_reuse_shader_ = std::make_unique<ComputeShader>("shaders/restir/gi_reuse.comp");

		auto setup_comp = [&](ComputeShader* s) {
			if (!s || !s->isValid()) return;
			s->use();
			GLuint temporal_idx = glGetUniformBlockIndex(s->ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) glUniformBlockBinding(s->ID, temporal_idx, Constants::UboBinding::TemporalData());
		};

		setup_comp(di_sampling_shader_.get());
		setup_comp(di_temporal_shader_.get());
		setup_comp(di_spatial_shader_.get());
		setup_comp(gi_trace_shader_.get());
		setup_comp(gi_reuse_shader_.get());
	}

	void RestirManager::CreateBuffers(int width, int height) {
		if (reservoir_buffers_[0]) glDeleteBuffers(2, reservoir_buffers_);
		if (gi_reservoir_buffers_[0]) glDeleteBuffers(2, gi_reservoir_buffers_);

		size_t size = width * height * sizeof(ReservoirCPU);
		glGenBuffers(2, reservoir_buffers_);
		glGenBuffers(2, gi_reservoir_buffers_);

		for (int i = 0; i < 2; i++) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, reservoir_buffers_[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, gi_reservoir_buffers_[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void RestirManager::Resize(int width, int height) {
		width_ = width;
		height_ = height;
		CreateBuffers(width, height);
	}

	void RestirManager::Update(int width, int height, float /* time */) {
		if (width != width_ || height != height_) {
			Resize(width, height);
		}
	}

	void RestirManager::Dispatch(
		GLuint depthTex,
		GLuint normalTex,
		GLuint velocityTex,
		GLuint colorTex,
		GLuint albedoTex,
		GLuint blueNoiseTex,
		LightManager& lightManager,
		FireEffectManager* fireManager
	) {
		PROJECT_PROFILE_SCOPE("RestirManager::Dispatch");
		if (width_ == 0 || height_ == 0) return;

		int num_lights = lightManager.GetActiveLightCount();
		int num_fire = fireManager ? Constants::Class::Particles::MaxParticles() : 0;

		// Bind AllLights and Particles
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AllLights(), lightManager.GetAllLightsSsbo());
		if (fireManager) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), fireManager->GetParticleBuffer());
		}

		auto bind_textures = [&](ComputeShader* s) {
			s->use();
			s->setInt("u_num_lights", num_lights);
			s->setInt("u_num_fire_particles", num_fire);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTex);
			s->setInt("gDepth", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, normalTex);
			s->setInt("gNormal", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTex);
			s->setInt("gVelocity", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, colorTex);
			s->setInt("gColor", 3);

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, albedoTex);
			s->setInt("gAlbedo", 4);

			if (blueNoiseTex) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
				glBindTexture(GL_TEXTURE_2D, blueNoiseTex);
				s->setInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
			}
		};

		// 1. DI Initial Sampling
		bind_textures(di_sampling_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), reservoir_buffers_[current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 2. DI Temporal Reuse
		bind_textures(di_temporal_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), reservoir_buffers_[current_buffer_index_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs1(), reservoir_buffers_[1 - current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 3. DI Spatial Reuse
		bind_textures(di_spatial_shader_.get());
		// Reuse temporal output as spatial input in-place-ish (or ping-pong)
		// For simplicity, we skip a third buffer and just do one spatial pass into reservoirs0
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 4. GI Trace
		bind_textures(gi_trace_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffers_[current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 5. GI Temporal Reuse
		bind_textures(gi_reuse_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffers_[current_buffer_index_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs1(), gi_reservoir_buffers_[1 - current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		current_buffer_index_ = 1 - current_buffer_index_;
	}

} // namespace Boidsish
