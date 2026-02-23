#include "post_processing/effects/RadianceCascadesEffect.h"

#include "constants.h"
#include "logger.h"
#include "shader.h"
#include <GL/glew.h>
#include <iostream>

namespace Boidsish {

	static void SetupEffectShaderBindings(ShaderBase& shader) {
		GLuint temporal_idx = glGetUniformBlockIndex(shader.ID, "TemporalData");
		if (temporal_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(shader.ID, temporal_idx, Constants::UboBinding::TemporalData());
		}
		GLuint lighting_idx = glGetUniformBlockIndex(shader.ID, "Lighting");
		if (lighting_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(shader.ID, lighting_idx, Constants::UboBinding::Lighting());
		}
	}

	namespace PostProcessing {

		RadianceCascadesEffect::RadianceCascadesEffect() {
			name_ = "Radiance Cascades";
			is_enabled_ = false; // Disabled by default as it's a heavy effect
		}

		RadianceCascadesEffect::~RadianceCascadesEffect() {
			if (cascades_texture_)
				glDeleteTextures(1, &cascades_texture_);
			if (hiz_texture_)
				glDeleteTextures(1, &hiz_texture_);
			if (history_textures_[0])
				glDeleteTextures(2, history_textures_);
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
			hiz_shader_ = std::make_unique<ComputeShader>("shaders/effects/hiz_gen.comp");
			accum_shader_ = std::make_unique<ComputeShader>("shaders/effects/radiance_cascades_accum.comp");

			if (gen_shader_->isValid())
				SetupEffectShaderBindings(*gen_shader_);
			if (merge_shader_->isValid())
				SetupEffectShaderBindings(*merge_shader_);
			if (composite_shader_->isValid())
				SetupEffectShaderBindings(*composite_shader_);
			if (hiz_shader_->isValid())
				SetupEffectShaderBindings(*hiz_shader_);
			if (accum_shader_->isValid())
				SetupEffectShaderBindings(*accum_shader_);

			InitializeResources();
		}

		void RadianceCascadesEffect::InitializeResources() {
			if (cascades_texture_)
				glDeleteTextures(1, &cascades_texture_);
			if (hiz_texture_)
				glDeleteTextures(1, &hiz_texture_);
			if (history_textures_[0])
				glDeleteTextures(2, history_textures_);

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

			// Hi-Z texture (min-downsampled depth)
			glGenTextures(1, &hiz_texture_);
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
			// We need 5 levels of mips for 1/2, 1/4, 1/8, 1/16, 1/32
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			// History textures for temporal accumulation
			glGenTextures(2, history_textures_);
			for (int i = 0; i < 2; ++i) {
				glBindTexture(GL_TEXTURE_2D, history_textures_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void RadianceCascadesEffect::GenerateHiZ(GLuint depthTexture) {
			if (!hiz_shader_ || !hiz_shader_->isValid())
				return;

			hiz_shader_->use();

			int   currW = width_;
			int   currH = height_;
			GLuint currInTex = depthTexture;

			for (int i = 0; i < 5; ++i) {
				int nextW = std::max(1, currW / 2);
				int nextH = std::max(1, currH / 2);

				hiz_shader_->setVec2("uSrcResolution", glm::vec2(currW, currH));
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, currInTex);
				hiz_shader_->setInt("inDepth", 0);

				glBindImageTexture(0, hiz_texture_, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

				glDispatchCompute((nextW + 7) / 8, (nextH + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				currInTex = hiz_texture_;
				// When using hiz_texture_ as input for next level, we need to bind specific mip level
				// But glActiveTexture + glBindTexture binds the whole texture.
				// For simplicity, we can use a second texture or just use imageLoad/Store for all levels.
				// Let's use imageLoad for Hi-Z gen for better control.
				currW = nextW;
				currH = nextH;
			}
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
			    !composite_shader_ || !composite_shader_->isValid() || !accum_shader_ || !accum_shader_->isValid())
				return;

			// 1. Generate Hi-Z if enabled
			if (enable_hiz_) {
				GenerateHiZ(depthTexture);
			}

			// 2. Generation Pass: For each cascade, trace rays
			gen_shader_->use();
			gen_shader_->setInt("uMaxSteps", max_steps_);
			gen_shader_->setVec2("uResolution", glm::vec2(width_, height_));
			gen_shader_->setBool("uEnableHiZ", enable_hiz_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			gen_shader_->setInt("uSceneTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			gen_shader_->setInt("uDepthTexture", 1);

			if (enable_hiz_) {
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				gen_shader_->setInt("uHizTexture", 2);
			}

			glBindImageTexture(0, cascades_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			// Dispatch for each cascade
			for (int i = 0; i < 4; ++i) {
				gen_shader_->setInt("uCascadeIndex", i);
				// Each cascade is 2W x 2H
				glDispatchCompute((width_ * 2 + 7) / 8, (height_ * 2 + 7) / 8, 1);
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 3. Merge Pass: Hierarchically merge cascades from 3 down to 0
			merge_shader_->use();
			merge_shader_->setVec2("uResolution", glm::vec2(width_, height_));

			// We need read/write access to the texture array
			glBindImageTexture(0, cascades_texture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

			for (int i = 2; i >= 0; --i) {
				merge_shader_->setInt("uCascadeIndex", i);
				glDispatchCompute((width_ * 2 + 7) / 8, (height_ * 2 + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// 4. Temporal Accumulation of Indirect Lighting
			// We use Cascade 0 as "CurrentFrame" but it needs to be averaged.
			// Actually, let's create a temporary texture for the averaged GI of this frame.
			// OR we can update the accum shader to handle averaging.
			// For now, let's just reuse history_textures_.

			int nextHistory = 1 - current_history_;
			accum_shader_->use();
			accum_shader_->setFloat("uAlpha", temporal_alpha_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, cascades_texture_);
			// We need a sampler for the array to get Cascade 0
			accum_shader_->setInt("uCascadesTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, history_textures_[current_history_]);
			accum_shader_->setInt("uHistoryFrame", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			accum_shader_->setInt("uVelocity", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			accum_shader_->setInt("uDepth", 3);

			glBindImageTexture(0, history_textures_[nextHistory], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			current_history_ = nextHistory;

			// 5. Composite Pass: Use Accumulated GI
			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uAccumulatedGI", 1);
			composite_shader_->setFloat("uIntensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, history_textures_[current_history_]);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void RadianceCascadesEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
