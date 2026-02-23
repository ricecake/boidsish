#include "post_processing/effects/RadianceCascadesEffect.h"

#include "logger.h"
#include "shader.h"
#include <GL/glew.h>
#include <iostream>

namespace Boidsish {
	namespace PostProcessing {

		RadianceCascadesEffect::RadianceCascadesEffect() {
			name_ = "Radiance Cascades";
			is_enabled_ = false; // Disabled by default as it's a heavy effect
		}

		RadianceCascadesEffect::~RadianceCascadesEffect() {
			if (cascades_texture_)
				glDeleteTextures(1, &cascades_texture_);
		}

		void RadianceCascadesEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			gen_shader_ = std::make_unique<ComputeShader>("shaders/effects/radiance_cascades_gen.comp");
			merge_shader_ = std::make_unique<ComputeShader>("shaders/effects/radiance_cascades_merge.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/radiance_cascades_composite.frag"
			);

			InitializeResources();
		}

		void RadianceCascadesEffect::InitializeResources() {
			if (cascades_texture_)
				glDeleteTextures(1, &cascades_texture_);

			glGenTextures(1, &cascades_texture_);
			glBindTexture(GL_TEXTURE_2D_ARRAY, cascades_texture_);

			// 4 cascades, each 2W x 2H
			// We use RGBA16F to store radiance
			glTexImage3D(
				GL_TEXTURE_2D_ARRAY,
				0,
				GL_RGBA16F,
				width_ * 2,
				height_ * 2,
				4, // 4 layers for 4 cascades
				0,
				GL_RGBA,
				GL_FLOAT,
				NULL
			);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}

		void RadianceCascadesEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           velocityTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			if (!gen_shader_ || !gen_shader_->isValid() || !merge_shader_ || !merge_shader_->isValid() ||
			    !composite_shader_ || !composite_shader_->isValid())
				return;

			// 1. Generation Pass: For each cascade, trace rays
			gen_shader_->use();
			gen_shader_->setInt("uMaxSteps", max_steps_);
			gen_shader_->setVec2("uResolution", glm::vec2(width_, height_));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			gen_shader_->setInt("uSceneTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			gen_shader_->setInt("uDepthTexture", 1);

			glBindImageTexture(0, cascades_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			// Dispatch for each cascade
			for (int i = 0; i < 4; ++i) {
				gen_shader_->setInt("uCascadeIndex", i);
				// Each cascade is 2W x 2H
				glDispatchCompute((width_ * 2 + 7) / 8, (height_ * 2 + 7) / 8, 1);
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Merge Pass: Hierarchically merge cascades from 3 down to 0
			merge_shader_->use();
			merge_shader_->setVec2("uResolution", glm::vec2(width_, height_));

			// We need read/write access to the texture array
			glBindImageTexture(0, cascades_texture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

			for (int i = 2; i >= 0; --i) {
				merge_shader_->setInt("uCascadeIndex", i);
				glDispatchCompute((width_ * 2 + 7) / 8, (height_ * 2 + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// 3. Composite Pass: Use merged Cascade 0
			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uCascadesTexture", 1);
			composite_shader_->setFloat("uIntensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D_ARRAY, cascades_texture_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void RadianceCascadesEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
