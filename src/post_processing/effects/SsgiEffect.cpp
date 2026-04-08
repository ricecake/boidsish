#include "post_processing/effects/SsgiEffect.h"

#include "logger.h"
#include "shader.h"
#include "constants.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SsgiEffect::SsgiEffect() {
			name_ = "SSGI";
			is_enabled_ = true;
		}

		SsgiEffect::~SsgiEffect() {
			if (ssgi_texture_)
				glDeleteTextures(1, &ssgi_texture_);
		}

		void SsgiEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			ssgi_shader_ = std::make_unique<ComputeShader>("shaders/effects/ssgi.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/ssgi_composite.frag"
			);

			// Set up static UBO bindings
			if (ssgi_shader_->isValid()) {
				GLuint lighting_idx = glGetUniformBlockIndex(ssgi_shader_->ID, "Lighting");
				if (lighting_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(ssgi_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
				}
				GLuint temporal_idx = glGetUniformBlockIndex(ssgi_shader_->ID, "TemporalData");
				if (temporal_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(ssgi_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
				}

				GLuint terrain_idx = glGetUniformBlockIndex(ssgi_shader_->ID, "TerrainData");
				if (terrain_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(ssgi_shader_->ID, terrain_idx, Constants::UboBinding::TerrainData());
				}

				GLuint probes_idx = glGetProgramResourceIndex(ssgi_shader_->ID, GL_SHADER_STORAGE_BLOCK, "TerrainProbes");
				if (probes_idx != GL_INVALID_INDEX) {
					glShaderStorageBlockBinding(ssgi_shader_->ID, probes_idx, Constants::SsboBinding::TerrainProbes());
				}

				GLuint biomes_idx = glGetUniformBlockIndex(ssgi_shader_->ID, "BiomeData");
				if (biomes_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(ssgi_shader_->ID, biomes_idx, Constants::UboBinding::Biomes());
				}
			}

			temporal_accumulator_.Initialize(width, height, GL_RGBA16F);

			InitializeTextures();
		}

		void SsgiEffect::InitializeTextures() {
			if (ssgi_texture_)
				glDeleteTextures(1, &ssgi_texture_);

			glGenTextures(1, &ssgi_texture_);
			glBindTexture(GL_TEXTURE_2D, ssgi_texture_);
			// SSGI at half resolution for performance
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_ / 2, height_ / 2, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void SsgiEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, const glm::mat4& /* viewMatrix */, const glm::mat4& /* projectionMatrix */, const glm::vec3& /* cameraPos */) {
			if (!ssgi_shader_ || !ssgi_shader_->isValid())
				return;

			// 1. Run SSGI compute shader
			ssgi_shader_->use();
			ssgi_shader_->setFloat("uRadius", radius_);
			ssgi_shader_->setFloat("uIntensity", intensity_);
			ssgi_shader_->setFloat("uDistanceFalloff", distance_falloff_);
			ssgi_shader_->setInt("uSteps", steps_);
			ssgi_shader_->setInt("uRayCount", ray_count_);
			ssgi_shader_->setFloat("uReflectionIntensity", reflection_intensity_);
			ssgi_shader_->setFloat("uRoughnessFactor", roughness_factor_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			ssgi_shader_->setInt("gDepth", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			ssgi_shader_->setInt("gColor", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			ssgi_shader_->setInt("gNormal", 2);

			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			ssgi_shader_->setInt("gVelocity", 6);

			if (blue_noise_texture_) {
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
				ssgi_shader_->setInt("u_blueNoiseTexture", 3);
			}

			if (hiz_texture_) {
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				ssgi_shader_->setInt("u_hizTexture", 4);
				ssgi_shader_->setInt("u_hizMipCount", hiz_mips_);
			}

			if (shadow_mask_texture_) {
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, shadow_mask_texture_);
				ssgi_shader_->setInt("uShadowMask", 5);
			}

			glBindImageTexture(0, ssgi_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((width_ / 2 + 7) / 8, (height_ / 2 + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Temporal Accumulation
			GLuint accumulatedSSGI = temporal_accumulator_.Accumulate(ssgi_texture_, velocityTexture, depthTexture);

			// 3. Composite
			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("ssgiTexture", 1);
			composite_shader_->setFloat("uIntensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, accumulatedSSGI);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SsgiEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			temporal_accumulator_.Resize(width, height);
			InitializeTextures();
		}

	} // namespace PostProcessing
} // namespace Boidsish
