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
			if (di_texture_) glDeleteTextures(1, &di_texture_);
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
				unified_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
				unified_shader_->bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());
				unified_shader_->bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
				unified_shader_->bindStorageBlock("TerrainProbes", Constants::SsboBinding::TerrainProbes());
				unified_shader_->bindUniformBlock("BiomeData", Constants::UboBinding::Biomes());
			}

			gi_ao_accumulator_.Initialize(internal_width_, internal_height_, GL_RGBA16F);
			di_accumulator_.Initialize(internal_width_, internal_height_, GL_RGBA16F);
			sss_accumulator_.Initialize(internal_width_, internal_height_, GL_R16F);

			gi_ao_accumulator_.SetAlpha(0.85f);
			di_accumulator_.SetAlpha(0.85f);
			sss_accumulator_.SetAlpha(0.95f);

			InitializeTextures();
		}

		void UnifiedScreenSpaceEffect::InitializeTextures() {
			if (gi_ao_texture_) glDeleteTextures(1, &gi_ao_texture_);
			if (di_texture_) glDeleteTextures(1, &di_texture_);
			if (sss_texture_) glDeleteTextures(1, &sss_texture_);

			glGenTextures(1, &gi_ao_texture_);
			glBindTexture(GL_TEXTURE_2D, gi_ao_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, internal_width_, internal_height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &di_texture_);
			glBindTexture(GL_TEXTURE_2D, di_texture_);
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
			unified_shader_->setBool("uRestirDIEnabled", restir_di_enabled_);
			unified_shader_->setBool("uRestirGIEnabled", restir_gi_enabled_);
			unified_shader_->setBool("uGTAOEnabled", gtao_enabled_);
			unified_shader_->setBool("uSSSEnabled", sss_enabled_);

			// Parameters
			unified_shader_->setFloat("uRestirDIIntensity", restir_di_intensity_);
			unified_shader_->setFloat("uRestirGIIntensity", restir_gi_intensity_);
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

			unified_shader_->setInt("u_num_lights", num_lights_);
			unified_shader_->setInt("u_num_fire_particles", num_fire_particles_);

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
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
				glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
				unified_shader_->setInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
			}

			if (hiz_texture_) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::HiZ());
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				unified_shader_->setInt("u_hizTexture", Constants::TextureUnit::HiZ());
				unified_shader_->setInt("u_hizMipCount", hiz_mips_);
			}

			if (di_reservoir_buffer_) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), di_reservoir_buffer_);
			}
			if (gi_reservoir_buffer_) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffer_);
			}

			if (all_lights_ssbo_) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AllLights(), all_lights_ssbo_);
			}
			if (particle_buffer_) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
			}

			// Use the accumulated shadow mask from the previous frame for SSGI coordination
			// We use Unit 9 as a scratch unit for post-fx specific accumulations
			GLuint prevShadowMask = sss_accumulator_.GetResult();
			if (prevShadowMask) {
				glActiveTexture(GL_TEXTURE9);
				glBindTexture(GL_TEXTURE_2D, prevShadowMask);
				unified_shader_->setInt("uShadowMask", 9);
			}

			glBindImageTexture(0, gi_ao_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindImageTexture(1, sss_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
			glBindImageTexture(2, di_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((internal_width_ + 7) / 8, (internal_height_ + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

			// Unbind image units to prevent stale bindings from interfering with
			// subsequent draw calls (e.g. grass rendering, bloom).
			glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
			glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

			GLuint accGIAO = gi_ao_accumulator_.Accumulate(gi_ao_texture_, velocityTexture, depthTexture);
			GLuint accDI = di_accumulator_.Accumulate(di_texture_, velocityTexture, depthTexture);
			GLuint accSSS = sss_accumulator_.Accumulate(sss_texture_, velocityTexture, depthTexture);

			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uGIAOTexture", 1);
			composite_shader_->setInt("uSSSTexture", 2);
			composite_shader_->setInt("uNormalTexture", 3);
			composite_shader_->setInt("uDepthTexture", 4);
			composite_shader_->setInt("uDITexture", 5);
			composite_shader_->setInt("uVelocityTexture", 6);
			composite_shader_->setInt("uRawGIAOTexture", 7);
			composite_shader_->setInt("uRawDITexture", 8);
			composite_shader_->setInt("uHistoryGIAOTexture", 9);
			composite_shader_->setInt("uHistoryDITexture", 10);

			composite_shader_->setBool("uSSGIEnabled", ssgi_enabled_);
			composite_shader_->setBool("uRestirDIEnabled", restir_di_enabled_);
			composite_shader_->setBool("uRestirGIEnabled", restir_gi_enabled_);
			composite_shader_->setBool("uGTAOEnabled", gtao_enabled_);
			composite_shader_->setBool("uSSSEnabled", sss_enabled_);

			composite_shader_->setFloat("uSSSIntensity", sss_intensity_);
			composite_shader_->setFloat("uGTAOIntensity", gtao_intensity_);
			composite_shader_->setFloat("uSSGIIntensity", ssgi_intensity_);
			composite_shader_->setFloat("uRestirDIIntensity", restir_di_intensity_);
			composite_shader_->setFloat("uRestirGIIntensity", restir_gi_intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, accGIAO);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, accSSS);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_2D, accDI);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D, gi_ao_texture_);
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_2D, di_texture_);
			glActiveTexture(GL_TEXTURE9);
			glBindTexture(GL_TEXTURE_2D, gi_ao_accumulator_.GetHistoryTexture());
			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_2D, di_accumulator_.GetHistoryTexture());

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void UnifiedScreenSpaceEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			internal_width_ = width / static_cast<int>(resolution_scale_);
			internal_height_ = height / static_cast<int>(resolution_scale_);
			gi_ao_accumulator_.Resize(internal_width_, internal_height_);
			di_accumulator_.Resize(internal_width_, internal_height_);
			sss_accumulator_.Resize(internal_width_, internal_height_);
			InitializeTextures();
		}

	} // namespace PostProcessing
} // namespace Boidsish
