#include "post_processing/effects/GtaoEffect.h"

#include "logger.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		GtaoEffect::GtaoEffect() {
			name_ = "GTAO";
			is_enabled_ = true;
		}

		GtaoEffect::~GtaoEffect() {
			if (ao_texture_)
				glDeleteTextures(1, &ao_texture_);
		}

		void GtaoEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			gtao_shader_ = std::make_unique<ComputeShader>("shaders/effects/gtao.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/ao_composite.frag"
			);
			temporal_accumulator_.Initialize(width, height, GL_RGBA16F);

			InitializeFBOs();
		}

		void GtaoEffect::InitializeFBOs() {
			if (ao_texture_)
				glDeleteTextures(1, &ao_texture_);

			glGenTextures(1, &ao_texture_);
			glBindTexture(GL_TEXTURE_2D, ao_texture_);
			// GTAO at half resolution for performance
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_ / 2, height_ / 2, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void GtaoEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint velocityTexture,
			const glm::mat4& /* viewMatrix */,
			const glm::mat4& /* projectionMatrix */,
			const glm::vec3& /* cameraPos */
		) {
			if (!gtao_shader_ || !gtao_shader_->isValid())
				return;

			// 1. Run GTAO compute shader
			gtao_shader_->use();
			gtao_shader_->setFloat("uRadius", radius_);
			gtao_shader_->setFloat("uIntensity", intensity_);
			gtao_shader_->setInt("uNumSteps", numSteps_);
			gtao_shader_->setInt("uNumDirections", numDirections_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			gtao_shader_->setInt("gDepth", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			gtao_shader_->setInt("gColor", 1);

			glBindImageTexture(0, ao_texture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

			glDispatchCompute((width_ / 2 + 7) / 8, (height_ / 2 + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Temporal Accumulation
			GLuint accumulatedAO = temporal_accumulator_.Accumulate(ao_texture_, velocityTexture, depthTexture);

			// 3. Composite (For now using SSAO composite shader)
			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("ssaoTexture", 1);
			composite_shader_->setFloat("intensity", 1.0f);
			composite_shader_->setFloat("power", 1.0f);
			composite_shader_->setFloat("uSSDIIntensity", ssdi_intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, accumulatedAO);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void GtaoEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			temporal_accumulator_.Resize(width, height);
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
