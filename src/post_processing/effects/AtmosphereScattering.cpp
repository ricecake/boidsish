#include "post_processing/effects/AtmosphereScattering.h"

#include "shader.h"
#include <iostream>

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereScattering::AtmosphereScattering() {}

		AtmosphereScattering::~AtmosphereScattering() {
			if (transmittance_lut_)
				glDeleteTextures(1, &transmittance_lut_);
			if (multi_scattering_lut_)
				glDeleteTextures(1, &multi_scattering_lut_);
			if (quad_vao_)
				glDeleteVertexArrays(1, &quad_vao_);
			if (quad_vbo_)
				glDeleteBuffers(1, &quad_vbo_);
		}

		void AtmosphereScattering::Initialize() {
			// Create LUT textures
			glGenTextures(1, &transmittance_lut_);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGB32F,
				transmittance_width_,
				transmittance_height_,
				0,
				GL_RGB,
				GL_FLOAT,
				NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &multi_scattering_lut_);
			glBindTexture(GL_TEXTURE_2D, multi_scattering_lut_);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGB32F,
				multi_scattering_size_,
				multi_scattering_size_,
				0,
				GL_RGB,
				GL_FLOAT,
				NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Load shaders
			transmittance_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/atmosphere/transmittance_lut.frag"
			);
			multi_scattering_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/atmosphere/multi_scattering_lut.frag"
			);

			// Setup quad
			float quad_vertices[] = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
									 -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};
			glGenVertexArrays(1, &quad_vao_);
			glBindVertexArray(quad_vao_);
			glGenBuffers(1, &quad_vbo_);
			glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glBindVertexArray(0);

			RebuildLUTs();
		}

		void AtmosphereScattering::Update(const Parameters& params) {
			if (params_ != params) {
				params_ = params;
				RebuildLUTs();
			}
		}

		void AtmosphereScattering::RebuildLUTs() {
			GenerateTransmittanceLUT();
			GenerateMultiScatteringLUT();
		}

		void AtmosphereScattering::GenerateTransmittanceLUT() {
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);

			GLuint fbo;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, transmittance_lut_, 0);

			glViewport(0, 0, transmittance_width_, transmittance_height_);
			glDisable(GL_DEPTH_TEST);
			transmittance_shader_->use();
			transmittance_shader_->setVec3("rayleighScattering", params_.rayleigh_scattering * params_.rayleigh_multiplier);
			transmittance_shader_->setFloat("rayleighScaleHeight", params_.rayleigh_scale_height);
			transmittance_shader_->setFloat("mieScattering", params_.mie_scattering * params_.mie_multiplier);
			transmittance_shader_->setFloat("mieExtinction", params_.mie_extinction * params_.mie_multiplier);
			transmittance_shader_->setFloat("mieScaleHeight", params_.mie_scale_height);
			transmittance_shader_->setFloat("mieAnisotropy", params_.mie_anisotropy);
			transmittance_shader_->setVec3("absorptionExtinction", params_.absorption_extinction);
			transmittance_shader_->setFloat("bottomRadius", params_.bottom_radius);
			transmittance_shader_->setFloat("topRadius", params_.top_radius);

			glBindVertexArray(quad_vao_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
			glEnable(GL_DEPTH_TEST);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		}

		void AtmosphereScattering::GenerateMultiScatteringLUT() {
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);

			GLuint fbo;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, multi_scattering_lut_, 0);

			glViewport(0, 0, multi_scattering_size_, multi_scattering_size_);
			glDisable(GL_DEPTH_TEST);
			multi_scattering_shader_->use();
			multi_scattering_shader_->setVec3("rayleighScattering", params_.rayleigh_scattering * params_.rayleigh_multiplier);
			multi_scattering_shader_->setFloat("rayleighScaleHeight", params_.rayleigh_scale_height);
			multi_scattering_shader_->setFloat("mieScattering", params_.mie_scattering * params_.mie_multiplier);
			multi_scattering_shader_->setFloat("mieExtinction", params_.mie_extinction * params_.mie_multiplier);
			multi_scattering_shader_->setFloat("mieAnisotropy", params_.mie_anisotropy);
			multi_scattering_shader_->setFloat("mieScaleHeight", params_.mie_scale_height);
			multi_scattering_shader_->setVec3("absorptionExtinction", params_.absorption_extinction);
			multi_scattering_shader_->setFloat("bottomRadius", params_.bottom_radius);
			multi_scattering_shader_->setFloat("topRadius", params_.top_radius);
			multi_scattering_shader_->setVec3("groundAlbedo", params_.ground_albedo);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			multi_scattering_shader_->setInt("transmittanceLUT", 0);

			glBindVertexArray(quad_vao_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
			glEnable(GL_DEPTH_TEST);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		}

	} // namespace PostProcessing
} // namespace Boidsish
