#include "post_processing/effects/SsrEffect.h"
#include "constants.h"
#include "logger.h"
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {
	namespace PostProcessing {

		SsrEffect::SsrEffect() {
			name_ = "SSR";
		}

		SsrEffect::~SsrEffect() {
			if (ssr_texture_) glDeleteTextures(1, &ssr_texture_);
			if (temporal_texture_) glDeleteTextures(1, &temporal_texture_);
			if (temporal_fbo_) glDeleteFramebuffers(1, &temporal_fbo_);
		}

		void SsrEffect::Initialize(int width, int height) {
			ssr_shader_ = std::make_unique<ComputeShader>("shaders/effects/ssr.comp");
			composite_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssr_composite.frag");
			passthrough_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");

			// Setup UBO bindings
			GLuint ssr_temporal_idx = glGetUniformBlockIndex(ssr_shader_->ID, "TemporalData");
			if (ssr_temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(ssr_shader_->ID, ssr_temporal_idx, Constants::UboBinding::TemporalData());
			}

			GLuint comp_temporal_idx = glGetUniformBlockIndex(composite_shader_->ID, "TemporalData");
			if (comp_temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(composite_shader_->ID, comp_temporal_idx, Constants::UboBinding::TemporalData());
			}

			CreateFBOs(width, height);
			width_ = width;
			height_ = height;
		}

		void SsrEffect::CreateFBOs(int width, int height) {
			if (ssr_texture_) glDeleteTextures(1, &ssr_texture_);
			if (temporal_texture_) glDeleteTextures(1, &temporal_texture_);
			if (temporal_fbo_) glDeleteFramebuffers(1, &temporal_fbo_);

			auto createTex = [&](GLuint& tex, GLenum format) {
				glGenTextures(1, &tex);
				glBindTexture(GL_TEXTURE_2D, tex);
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			};

			createTex(ssr_texture_, GL_RGBA16F);
			createTex(temporal_texture_, GL_RGBA16F);

			glGenFramebuffers(1, &temporal_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, temporal_fbo_);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temporal_texture_, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				logger::ERROR("SSR Temporal FBO is not complete!");

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void SsrEffect::Apply(const PostProcessingContext& context) {
			if (!ssr_shader_->isValid() || !composite_shader_->isValid()) return;

			// 1. Raymarching pass (Compute)
			ssr_shader_->use();
			ssr_shader_->setMat4("uProjection", context.projectionMatrix);
			ssr_shader_->setMat4("uInvProjection", context.invProjectionMatrix);
			ssr_shader_->setMat4("uView", context.viewMatrix);
			ssr_shader_->setMat4("uInvView", context.invViewMatrix);
			ssr_shader_->setVec2("uScreenSize", (float)width_, (float)height_);
			ssr_shader_->setFloat("uTime", context.time);
			ssr_shader_->setFloat("uMaxDistance", max_distance_);
			ssr_shader_->setFloat("uStride", stride_);
			ssr_shader_->setFloat("uThickness", thickness_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, context.depthTexture);
			ssr_shader_->setInt("uDepthTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, context.normalRoughnessTexture);
			ssr_shader_->setInt("uNormalRoughnessTexture", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, context.sourceTexture);
			ssr_shader_->setInt("uSceneTexture", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, context.hizTexture);
			ssr_shader_->setInt("uHiZTexture", 3);

			glBindImageTexture(0, ssr_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Simple Temporal Accumulation (Denoising)
			// Blit current SSR to temporal texture mixed with previous
			glBindFramebuffer(GL_FRAMEBUFFER, temporal_fbo_);
			glViewport(0, 0, width_, height_);
			glEnable(GL_BLEND);
			glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
			glBlendColor(0, 0, 0, 0.1f); // Accumulation alpha: 0.1 (stronger denoising)

			passthrough_shader_->use();
			passthrough_shader_->setInt("sceneTexture", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ssr_texture_);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glDisable(GL_BLEND);
			// glBindFramebuffer(GL_FRAMEBUFFER, 0); // REMOVED: Breaking post-processing chain

			// 3. Composite pass
			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uSsrTexture", 1);
			composite_shader_->setInt("uAlbedoMetallicTexture", 2);
			composite_shader_->setFloat("uIntensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, context.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, temporal_texture_);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, context.albedoMetallicTexture);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, context.depthTexture);
			composite_shader_->setInt("uDepthTexture", 3);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, context.normalRoughnessTexture);
			composite_shader_->setInt("uNormalRoughnessTexture", 4);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SsrEffect::Resize(int width, int height) {
			if (width == width_ && height == height_) return;
			CreateFBOs(width, height);
			width_ = width;
			height_ = height;
		}

	}
}
