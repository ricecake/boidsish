#include "post_processing/effects/VolumetricLightingEffect.h"

#include <algorithm>
#include <array>
#include <iostream>

#include "constants.h"
#include "shader.h"
#include "light.h"

namespace Boidsish {
	namespace PostProcessing {

		VolumetricLightingEffect::VolumetricLightingEffect() {
			name_ = "Volumetric Lighting";
		}

		VolumetricLightingEffect::~VolumetricLightingEffect() {
			if (epipolar_tex_)
				glDeleteTextures(1, &epipolar_tex_);
			if (volumetric_tex_)
				glDeleteTextures(1, &volumetric_tex_);
			if (blurred_tex_)
				glDeleteTextures(1, &blurred_tex_);
		}

		void VolumetricLightingEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			epipolar_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_lighting_raymarch.comp");
			reproject_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_lighting_reproject.comp");
			blur_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_lighting_blur.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/volumetric_lighting_composite.frag"
			);

			auto setup_shader = [](ShaderBase& s) {
				s.use();
				GLuint lighting_idx = glGetUniformBlockIndex(s.ID, "Lighting");
				if (lighting_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, lighting_idx, Constants::UboBinding::Lighting());
				}
				GLuint shadows_idx = glGetUniformBlockIndex(s.ID, "Shadows");
				if (shadows_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, shadows_idx, Constants::UboBinding::Shadows());
				}
				s.trySetInt("shadowMaps", 10);
			};

			setup_shader(*epipolar_shader_);
			setup_shader(*reproject_shader_);
			setup_shader(*composite_shader_);

			InitializeResources();
		}

		void VolumetricLightingEffect::InitializeResources() {
			if (epipolar_tex_)
				glDeleteTextures(1, &epipolar_tex_);
			if (volumetric_tex_)
				glDeleteTextures(1, &volumetric_tex_);
			if (blurred_tex_)
				glDeleteTextures(1, &blurred_tex_);

			glGenTextures(1, &epipolar_tex_);
			glBindTexture(GL_TEXTURE_2D, epipolar_tex_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, samples_per_line_, num_lines_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &volumetric_tex_);
			glBindTexture(GL_TEXTURE_2D, volumetric_tex_);
			// Reconstruct at half res for performance
			int low_w = std::max(1, width_ / 2);
			int low_h = std::max(1, height_ / 2);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_w, low_h, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &blurred_tex_);
			glBindTexture(GL_TEXTURE_2D, blurred_tex_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_w, low_h, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		void VolumetricLightingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeResources();
		}

		void VolumetricLightingEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			// Pass significant light indices to the shader
			auto significantLights = GetSignificantLights(cameraPos);
			int  numSignificant = std::min((int)significantLights.size(), 4);

			if (numSignificant <= 0) {
				// Blit source to target anyway to avoid black screen
				composite_shader_->use();
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				composite_shader_->setInt("sceneTexture", 0);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, 0); // No volumetric
				composite_shader_->setInt("volumetricTexture", 1);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				return;
			}

			// 1. Raymarch Pass (Low-Res 2D Grid)
			epipolar_shader_->use();
			epipolar_shader_->setMat4("uInvView", glm::inverse(viewMatrix));
			epipolar_shader_->setMat4("uInvProj", glm::inverse(projectionMatrix));
			epipolar_shader_->setVec3("uCameraPos", cameraPos);
			epipolar_shader_->setFloat("uScatteringCoef", scattering_coef_);
			epipolar_shader_->setFloat("uAbsorptionCoef", absorption_coef_);
			epipolar_shader_->setFloat("uPhaseG", phase_g_);
			epipolar_shader_->setFloat("uHazeDensity", haze_density_);
			epipolar_shader_->setFloat("uHazeHeight", haze_height_);
			epipolar_shader_->setFloat("uTime", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			epipolar_shader_->setInt("uDepthTex", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, noise_textures_.blue_noise);
			epipolar_shader_->setInt("u_blueNoiseTexture", 1);

			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.noise);
			epipolar_shader_->setInt("u_noiseTexture", 5);

			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array_);
			epipolar_shader_->setInt("shadowMaps", 10);

			// Pass shadow map indices
			std::array<int, 10> shadow_indices;
			shadow_indices.fill(-1);
			for (size_t j = 0; j < lights_.size() && j < 10; ++j) {
				shadow_indices[j] = lights_[j].shadow_map_index;
			}
			epipolar_shader_->setIntArray("lightShadowIndices", shadow_indices.data(), 10);

			glBindImageTexture(0, volumetric_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glm::vec4 lightIndices(-1.0f);
			for (int i = 0; i < numSignificant; ++i) {
				lightIndices[i] = (float)significantLights[i].index;
			}
			epipolar_shader_->setVec4("uLightIndices", lightIndices);
			epipolar_shader_->setInt("uNumLights", numSignificant);

			int low_w = std::max(1, width_ / 2);
			int low_h = std::max(1, height_ / 2);
			glDispatchCompute((low_w + 15) / 16, (low_h + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 3. Bilateral Blur Pass
			blur_shader_->use();
			blur_shader_->setVec2("uScreenSize", glm::vec2(width_ / 2, height_ / 2));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, volumetric_tex_);
			blur_shader_->setInt("uInputTex", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			blur_shader_->setInt("uDepthTex", 1);

			glBindImageTexture(0, blurred_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute((width_ / 2 + 15) / 16, (height_ / 2 + 15) / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 4. Composite
			composite_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			composite_shader_->setInt("sceneTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, blurred_tex_);
			composite_shader_->setInt("volumetricTexture", 1);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		std::vector<VolumetricLightingEffect::LightMetadata> VolumetricLightingEffect::GetSignificantLights(const glm::vec3& cameraPos) {
			std::vector<LightMetadata> significant;

			// Heuristic: intensity * aerosol_density > threshold
			// This ensures we don't march lights that wouldn't be visible in the current fog
			float visibilityThreshold = 0.05f;

			for (size_t i = 0; i < lights_.size() && i < 10; ++i) {
				const auto& l = lights_[i];
				if (!l.casts_shadow)
					continue;

				float weight = l.intensity * haze_density_;
				// Boost weight for directional lights as they are global
				if (l.type == DIRECTIONAL_LIGHT)
					weight *= 2.0f;

				if (weight < visibilityThreshold)
					continue;

				significant.push_back({(int)i, weight});
			}

			std::sort(significant.begin(), significant.end(), [](const auto& a, const auto& b) {
				return a.weight > b.weight;
			});

			return significant;
		}

	} // namespace PostProcessing
} // namespace Boidsish
