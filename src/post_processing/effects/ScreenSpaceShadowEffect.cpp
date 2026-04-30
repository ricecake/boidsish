#include "post_processing/effects/ScreenSpaceShadowEffect.h"
#include "shader.h"
#include "logger.h"
#include "constants.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		ScreenSpaceShadowEffect::ScreenSpaceShadowEffect() {
			name_ = "ScreenSpaceShadows";
			is_enabled_ = true;
		}

		ScreenSpaceShadowEffect::~ScreenSpaceShadowEffect() {
			if (shadow_mask_texture_)
				glDeleteTextures(1, &shadow_mask_texture_);
		}

		void ScreenSpaceShadowEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			sss_shader_ = std::make_unique<ComputeShader>("shaders/effects/screen_space_shadows.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/sss_composite.frag"
			);

			// Set up static UBO bindings
			if (sss_shader_->isValid()) {
				GLuint lighting_idx = glGetUniformBlockIndex(sss_shader_->ID, "Lighting");
				if (lighting_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(sss_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
				}
				GLuint temporal_idx = glGetUniformBlockIndex(sss_shader_->ID, "TemporalData");
				if (temporal_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(sss_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
				}
			}

			temporal_accumulator_.Initialize(width, height, GL_R16F);

			InitializeTextures();
		}

		void ScreenSpaceShadowEffect::InitializeTextures() {
			if (shadow_mask_texture_)
				glDeleteTextures(1, &shadow_mask_texture_);

			glGenTextures(1, &shadow_mask_texture_);
			glBindTexture(GL_TEXTURE_2D, shadow_mask_texture_);
			// Shadow mask at full resolution for fine detail
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void ScreenSpaceShadowEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			if (!sss_shader_ || !sss_shader_->isValid())
				return;

			// 1. Run SSS compute shader
			sss_shader_->use();
			sss_shader_->setFloat("uIntensity", intensity_);
			sss_shader_->setFloat("uRadius", radius_);
			sss_shader_->setFloat("uBias", bias_);
			sss_shader_->setInt("uSteps", steps_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			sss_shader_->setInt("uDepth", 0);

			if (blue_noise_texture_) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
				sss_shader_->setInt("u_blueNoiseTexture", 1);
			}

			glBindImageTexture(0, shadow_mask_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

			glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
			// Correct memory barrier for texture sampling after compute write
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Temporal Accumulation
			GLuint accumulatedShadow = temporal_accumulator_.Accumulate(
				shadow_mask_texture_,
				velocityTexture,
				depthTexture
			);

			// 3. Composite
			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uShadowMask", 1);
			composite_shader_->setFloat("uIntensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, accumulatedShadow);

			// Note: PostProcessingManager binds a quad VAO before calling Apply()
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void ScreenSpaceShadowEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			temporal_accumulator_.Resize(width, height);
			InitializeTextures();
		}

	} // namespace PostProcessing
} // namespace Boidsish
