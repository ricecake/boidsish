#include "post_processing/effects/SssrEffect.h"

#include "constants.h"
#include "logger.h"
#include <GL/glew.h>
#include <cmath>
#include <algorithm>

namespace Boidsish {
	namespace PostProcessing {

		SssrEffect::SssrEffect(const GLuint& hizTexture, const int& hizLevels):
			hi_z_texture_(hizTexture), hi_z_levels_(hizLevels) {
			name_ = "SSSR";
			is_enabled_ = true;
		}

		SssrEffect::~SssrEffect() {
			if (trace_texture_)
				glDeleteTextures(1, &trace_texture_);
			if (filter_texture_)
				glDeleteTextures(1, &filter_texture_);
		}

		void SssrEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			sssr_shader_ = std::make_unique<ComputeShader>("shaders/effects/sssr_trace.comp");
			spatial_filter_shader_ = std::make_unique<ComputeShader>("shaders/effects/sssr_spatial_filter.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/sssr_composite.frag"
			);

			// Bind standard UBOs for SSSR shader
			sssr_shader_->use();
			GLuint lighting_idx = glGetUniformBlockIndex(sssr_shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(sssr_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint temporal_idx = glGetUniformBlockIndex(sssr_shader_->ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(sssr_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
			}

			// Bind TemporalData to Spatial Filter
			spatial_filter_shader_->use();
			GLuint sf_temporal_idx = glGetUniformBlockIndex(spatial_filter_shader_->ID, "TemporalData");
			if (sf_temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(spatial_filter_shader_->ID, sf_temporal_idx, Constants::UboBinding::TemporalData());
			}

			temporal_accumulator_.Initialize(width, height, GL_RGBA16F);

			InitializeFBOs();
		}

		void SssrEffect::InitializeFBOs() {
			if (trace_texture_)
				glDeleteTextures(1, &trace_texture_);
			if (filter_texture_)
				glDeleteTextures(1, &filter_texture_);

			// Trace Texture (Result of ray marching)
			glGenTextures(1, &trace_texture_);
			glBindTexture(GL_TEXTURE_2D, trace_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Filter Texture (Result of spatial filter)
			glGenTextures(1, &filter_texture_);
			glBindTexture(GL_TEXTURE_2D, filter_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void SssrEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           velocityTexture,
			GLuint           normalTexture,
			GLuint           materialTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			if (!is_enabled_)
				return;
			frame_count_++;

			// 1. Stochastic Trace
			if (sssr_shader_ && sssr_shader_->isValid()) {
				sssr_shader_->use();
				sssr_shader_->setMat4("uView", viewMatrix);
				sssr_shader_->setMat4("uProj", projectionMatrix);
				sssr_shader_->setMat4("uInvView", glm::inverse(viewMatrix));
				sssr_shader_->setMat4("uInvProj", glm::inverse(projectionMatrix));
				sssr_shader_->setVec3("uCameraPos", cameraPos);
				sssr_shader_->setFloat("uIntensity", intensity_);
				sssr_shader_->setInt("uMaxSteps", max_steps_);
				sssr_shader_->setFloat("uRoughnessThreshold", roughness_threshold_);
				sssr_shader_->setInt("uFrameCount", int(frame_count_));
				sssr_shader_->setInt("uNumMips", hi_z_levels_);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, hi_z_texture_);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, normalTexture);
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, materialTexture);

				glBindImageTexture(0, trace_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

				glDispatchCompute((width_ + 15) / 16, (height_ + 15) / 16, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// 3. Spatial Filter
			if (spatial_filter_shader_ && spatial_filter_shader_->isValid()) {
				spatial_filter_shader_->use();
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, trace_texture_);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, normalTexture);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				glBindImageTexture(0, filter_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

				glDispatchCompute((width_ + 15) / 16, (height_ + 15) / 16, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			} else {
				// Fallback if shader fails
				glCopyImageSubData(trace_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
								   filter_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
								   width_, height_, 1);
			}

			// 4. Temporal Accumulation
			GLuint accumulatedSSR = temporal_accumulator_.Accumulate(filter_texture_, velocityTexture, depthTexture);

			// 5. Composite
			if (composite_shader_ && composite_shader_->isValid()) {
				composite_shader_->use();
				composite_shader_->setInt("uSceneTexture", 0);
				composite_shader_->setInt("uSssrTexture", 1);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, accumulatedSSR);

				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}

		void SssrEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			temporal_accumulator_.Resize(width, height);
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
