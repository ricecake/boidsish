#include "post_processing/effects/UnifiedScreenSpaceEffect.h"

#include "logger.h"
#include "shader.h"
#include "constants.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		UnifiedScreenSpaceEffect::UnifiedScreenSpaceEffect() {
			name_ = "UnifiedScreenSpace";
			is_enabled_ = true;
		}

		UnifiedScreenSpaceEffect::~UnifiedScreenSpaceEffect() {
			if (gi_ao_texture_) glDeleteTextures(1, &gi_ao_texture_);
			if (sss_texture_) glDeleteTextures(1, &sss_texture_);
		}

		void UnifiedScreenSpaceEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			internal_width_ = width / static_cast<int>(resolution_scale_);
			internal_height_ = height / static_cast<int>(resolution_scale_);

			unified_shader_ = std::make_unique<ComputeShader>("shaders/effects/unified_screen_space.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/unified_screen_space_composite.frag"
			);

			if (unified_shader_->isValid()) {
				GLuint lighting_idx = glGetUniformBlockIndex(unified_shader_->ID, "Lighting");
				if (lighting_idx != GL_INVALID_INDEX) glUniformBlockBinding(unified_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());

				GLuint temporal_idx = glGetUniformBlockIndex(unified_shader_->ID, "TemporalData");
				if (temporal_idx != GL_INVALID_INDEX) glUniformBlockBinding(unified_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());

				GLuint terrain_idx = glGetUniformBlockIndex(unified_shader_->ID, "TerrainData");
				if (terrain_idx != GL_INVALID_INDEX) glUniformBlockBinding(unified_shader_->ID, terrain_idx, Constants::UboBinding::TerrainData());

				GLuint probes_idx = glGetProgramResourceIndex(unified_shader_->ID, GL_SHADER_STORAGE_BLOCK, "TerrainProbes");
				if (probes_idx != GL_INVALID_INDEX) glShaderStorageBlockBinding(unified_shader_->ID, probes_idx, Constants::SsboBinding::TerrainProbes());

				GLuint biomes_idx = glGetUniformBlockIndex(unified_shader_->ID, "BiomeData");
				if (biomes_idx != GL_INVALID_INDEX) glUniformBlockBinding(unified_shader_->ID, biomes_idx, Constants::UboBinding::Biomes());
			}

			gi_ao_accumulator_.Initialize(internal_width_, internal_height_, GL_RGBA16F);
			sss_accumulator_.Initialize(internal_width_, internal_height_, GL_R16F);

			InitializeTextures();
		}

		void UnifiedScreenSpaceEffect::InitializeTextures() {
			if (gi_ao_texture_) glDeleteTextures(1, &gi_ao_texture_);
			if (sss_texture_) glDeleteTextures(1, &sss_texture_);

			glGenTextures(1, &gi_ao_texture_);
			glBindTexture(GL_TEXTURE_2D, gi_ao_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, internal_width_, internal_height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &sss_texture_);
			glBindTexture(GL_TEXTURE_2D, sss_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, internal_width_, internal_height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void UnifiedScreenSpaceEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& /* viewMatrix */, const glm::mat4& /* projectionMatrix */, const glm::vec3& /* cameraPos */) {
			if (!unified_shader_ || !unified_shader_->isValid()) return;

			unified_shader_->use();

			// Toggles
			unified_shader_->setBool("uSSGIEnabled", ssgi_enabled_);
			unified_shader_->setBool("uGTAOEnabled", gtao_enabled_);
			unified_shader_->setBool("uSSSEnabled", sss_enabled_);

			// Parameters
			unified_shader_->setFloat("uSSGIIntensity", ssgi_intensity_);
			unified_shader_->setFloat("uSSGIRadius", ssgi_radius_);
			unified_shader_->setFloat("uSSGIDistanceFalloff", ssgi_falloff_);
			unified_shader_->setInt("uSSGISteps", ssgi_steps_);
			unified_shader_->setInt("uSSGIRayCount", ssgi_ray_count_);
			unified_shader_->setFloat("uSSGIReflectionIntensity", ssgi_reflection_intensity_);
			unified_shader_->setFloat("uSSGIRoughnessFactor", ssgi_roughness_factor_);

			unified_shader_->setFloat("uGTAOIntensity", gtao_intensity_);
			unified_shader_->setFloat("uGTAORadius", gtao_radius_);
			unified_shader_->setFloat("uGTAOFalloff", gtao_falloff_);
			unified_shader_->setInt("uGTAOSteps", gtao_steps_);
			unified_shader_->setInt("uGTAODirections", gtao_directions_);

			unified_shader_->setFloat("uSSSIntensity", sss_intensity_);
			unified_shader_->setFloat("uSSSRadius", sss_radius_);
			unified_shader_->setFloat("uSSSBias", sss_bias_);
			unified_shader_->setInt("uSSSSteps", sss_steps_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			unified_shader_->setInt("gDepth", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			unified_shader_->setInt("gColor", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			unified_shader_->setInt("gNormal", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			unified_shader_->setInt("gVelocity", 3);

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, albedoTexture);
			unified_shader_->setInt("gAlbedo", 4);

			if (blue_noise_texture_) {
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
				unified_shader_->setInt("u_blueNoiseTexture", 5);
			}

			if (hiz_texture_) {
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				unified_shader_->setInt("u_hizTexture", 6);
				unified_shader_->setInt("u_hizMipCount", hiz_mips_);
			}

			// Use the accumulated shadow mask from the previous frame for SSGI coordination
			GLuint prevShadowMask = sss_accumulator_.GetResult();
			if (prevShadowMask) {
				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D, prevShadowMask);
				unified_shader_->setInt("uShadowMask", 7);
			}

			glBindImageTexture(0, gi_ao_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindImageTexture(1, sss_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

			glDispatchCompute((internal_width_ + 7) / 8, (internal_height_ + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

			// Unbind image units to prevent stale bindings from interfering with
			// subsequent draw calls (e.g. grass rendering, bloom).
			glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

			GLuint accGIAO = gi_ao_accumulator_.Accumulate(gi_ao_texture_, velocityTexture, depthTexture);
			GLuint accSSS = sss_accumulator_.Accumulate(sss_texture_, velocityTexture, depthTexture);

			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uGIAOTexture", 1);
			composite_shader_->setInt("uSSSTexture", 2);
			composite_shader_->setInt("uNormalTexture", 3);

			composite_shader_->setBool("uSSGIEnabled", ssgi_enabled_);
			composite_shader_->setBool("uGTAOEnabled", gtao_enabled_);
			composite_shader_->setBool("uSSSEnabled", sss_enabled_);
			composite_shader_->setFloat("uSSSIntensity", sss_intensity_);
			composite_shader_->setFloat("uGTAOIntensity", gtao_intensity_);
			composite_shader_->setFloat("uSSGIIntensity", ssgi_intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, accGIAO);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, accSSS);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, normalTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void UnifiedScreenSpaceEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			internal_width_ = width / static_cast<int>(resolution_scale_);
			internal_height_ = height / static_cast<int>(resolution_scale_);
			gi_ao_accumulator_.Resize(internal_width_, internal_height_);
			sss_accumulator_.Resize(internal_width_, internal_height_);
			InitializeTextures();
		}

	} // namespace PostProcessing
} // namespace Boidsish
