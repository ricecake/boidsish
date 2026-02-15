#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/SssrEffect.h"

#include "logger.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		SssrEffect::SssrEffect() {
			name_ = "SSSR";
			is_enabled_ = true;
		}

		SssrEffect::~SssrEffect() {
			if (reflection_texture_)
				glDeleteTextures(1, &reflection_texture_);
			if (filtered_reflection_texture_)
				glDeleteTextures(1, &filtered_reflection_texture_);
			if (history_texture_)
				glDeleteTextures(1, &history_texture_);
		}

		void SssrEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			sssr_shader_ = std::make_unique<ComputeShader>("shaders/post_processing/sssr.comp");
			spatial_filter_shader_ = std::make_unique<ComputeShader>("shaders/post_processing/sssr_spatial_filter.comp");
			temporal_accumulation_shader_ = std::make_unique<ComputeShader>(
				"shaders/post_processing/sssr_temporal_accumulation.comp"
			);
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/sssr_composite.frag"
			);

			InitializeFBOs();
		}

		void SssrEffect::InitializeFBOs() {
			if (reflection_texture_)
				glDeleteTextures(1, &reflection_texture_);
			if (filtered_reflection_texture_)
				glDeleteTextures(1, &filtered_reflection_texture_);
			if (history_texture_)
				glDeleteTextures(1, &history_texture_);

			glGenTextures(1, &reflection_texture_);
			glBindTexture(GL_TEXTURE_2D, reflection_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &filtered_reflection_texture_);
			glBindTexture(GL_TEXTURE_2D, filtered_reflection_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glGenTextures(1, &history_texture_);
			glBindTexture(GL_TEXTURE_2D, history_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		void SssrEffect::Calculate(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix
		) {
			if (!is_enabled_) return;
			frame_count_++;

			// 1. SSSR Raymarching
			sssr_shader_->use();
			sssr_shader_->setMat4("uView", viewMatrix);
			sssr_shader_->setMat4("uProjection", projectionMatrix);
			sssr_shader_->setMat4("uInvView", glm::inverse(viewMatrix));
			sssr_shader_->setMat4("uInvProjection", glm::inverse(projectionMatrix));
			sssr_shader_->setFloat("uTime", (float)frame_count_ * 0.016f);
			sssr_shader_->setInt("uFrameCount", (int)frame_count_);
			sssr_shader_->setInt("uMaxSamples", max_samples_);
			sssr_shader_->setFloat("uRoughnessThreshold", roughness_threshold_);
			sssr_shader_->setFloat("uMirrorThreshold", mirror_threshold_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gbuffer.normal);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gbuffer.material);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, gbuffer.hiz);

			glBindImageTexture(0, reflection_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((width_ + 15) / 16, (height_ + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Spatial Filter
			spatial_filter_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, reflection_texture_);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gbuffer.normal);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);

			glBindImageTexture(0, filtered_reflection_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((width_ + 15) / 16, (height_ + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 3. Temporal Accumulation
			temporal_accumulation_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, filtered_reflection_texture_);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, history_texture_);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gbuffer.velocity);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);

			// Re-use reflection_texture_ for output of temporal (ping-pong)
			glBindImageTexture(0, reflection_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((width_ + 15) / 16, (height_ + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Update history
			glCopyImageSubData(
				reflection_texture_,
				GL_TEXTURE_2D,
				0,
				0,
				0,
				0,
				history_texture_,
				GL_TEXTURE_2D,
				0,
				0,
				0,
				0,
				width_,
				height_,
				1
			);
		}

		void SssrEffect::Composite(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& projectionMatrix
		) {
			if (!is_enabled_) return;

			// 4. Composite
			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("reflectionTexture", 1);
			composite_shader_->setInt("gNormal", 2);
			composite_shader_->setInt("gMaterial", 3);
			composite_shader_->setInt("gDepth", 4);
			composite_shader_->setMat4("uInvProjection", glm::inverse(projectionMatrix));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, reflection_texture_);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gbuffer.normal);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, gbuffer.material);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SssrEffect::Apply(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& /* cameraPos */
		) {
			Calculate(sourceTexture, gbuffer, viewMatrix, projectionMatrix);
			Composite(sourceTexture, gbuffer, projectionMatrix);
		}

		void SssrEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
