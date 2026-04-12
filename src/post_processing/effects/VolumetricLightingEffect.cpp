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

			grid_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_lighting_raymarch.comp");
			epipolar_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_lighting_epipolar.comp");
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

			setup_shader(*grid_shader_);
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

			samples_per_line_ = 512;
			num_lines_ = 256;

			glGenTextures(1, &epipolar_tex_);
			glBindTexture(GL_TEXTURE_2D, epipolar_tex_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, samples_per_line_, num_lines_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenTextures(1, &volumetric_tex_);
			glBindTexture(GL_TEXTURE_2D, volumetric_tex_);
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
			auto significantLights = GetSignificantLights(cameraPos);
			int  numSignificant = std::min((int)significantLights.size(), 8);

			if (numSignificant <= 0) {
				composite_shader_->use();
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);
				composite_shader_->setInt("sceneTexture", 0);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, 0);
				composite_shader_->setInt("volumetricTexture", 1);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				return;
			}

			int low_w = std::max(1, width_ / 2);
			int low_h = std::max(1, height_ / 2);

			ComputeShader* active_shader = (technique_ == VolumetricTechnique::Grid) ? grid_shader_.get() : epipolar_shader_.get();

			active_shader->use();
			active_shader->setMat4("uInvView", glm::inverse(viewMatrix));
			active_shader->setMat4("uInvProj", glm::inverse(projectionMatrix));
			active_shader->setMat4("uViewProj", projectionMatrix * viewMatrix);
			active_shader->setVec3("uCameraPos", cameraPos);
			active_shader->setFloat("uScatteringCoef", scattering_coef_);
			active_shader->setFloat("uAbsorptionCoef", absorption_coef_);
			active_shader->setFloat("uPhaseG", phase_g_);
			active_shader->setFloat("uHazeDensity", haze_density_);
			active_shader->setFloat("uHazeHeight", haze_height_);
			active_shader->setFloat("uIntensity", intensity_);
			active_shader->setFloat("uTime", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			active_shader->setInt("uDepthTex", 0);

			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D, noise_textures_.blue_noise);
			active_shader->setInt("u_blueNoiseTexture", 7);

			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.noise);
			active_shader->setInt("u_noiseTexture", 5);

			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array_);
			active_shader->setInt("shadowMaps", 10);

			std::array<int, 10> shadow_indices;
			shadow_indices.fill(-1);
			for (size_t j = 0; j < lights_.size() && j < 10; ++j) {
				shadow_indices[j] = lights_[j].shadow_map_index;
			}
			active_shader->setIntArray("lightShadowIndices", shadow_indices.data(), 10);

			glm::vec4 lightIndicesA(-1.0f);
			glm::vec4 lightIndicesB(-1.0f);
			for (int i = 0; i < numSignificant; ++i) {
				if (i < 4) lightIndicesA[i] = (float)significantLights[i].index;
				else lightIndicesB[i - 4] = (float)significantLights[i].index;
			}
			active_shader->setVec4("uLightIndicesA", lightIndicesA);
			active_shader->setVec4("uLightIndicesB", lightIndicesB);
			active_shader->setInt("uNumLights", numSignificant);

			if (technique_ == VolumetricTechnique::Grid) {
				glBindImageTexture(0, volumetric_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glDispatchCompute((low_w + 15) / 16, (low_h + 15) / 16, 1);
			} else {
				glBindImageTexture(0, epipolar_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glDispatchCompute(samples_per_line_ / 16, num_lines_ / 16, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				reproject_shader_->use();
				reproject_shader_->setMat4("uViewProj", projectionMatrix * viewMatrix);
				reproject_shader_->setVec3("uCameraPos", cameraPos);
				reproject_shader_->setVec4("uLightIndicesA", lightIndicesA);
				reproject_shader_->setVec2("uScreenSize", glm::vec2(low_w, low_h));
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, epipolar_tex_);
				reproject_shader_->setInt("uEpipolarTex", 0);
				glBindImageTexture(0, volumetric_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glDispatchCompute((low_w + 15) / 16, (low_h + 15) / 16, 1);
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 3. Bilateral Blur Pass
			blur_shader_->use();
			blur_shader_->setVec2("uScreenSize", glm::vec2(low_w, low_h));
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, volumetric_tex_);
			blur_shader_->setInt("uInputTex", 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			blur_shader_->setInt("uDepthTex", 1);
			glBindImageTexture(0, blurred_tex_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((low_w + 15) / 16, (low_h + 15) / 16, 1);
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
			float                      visibilityThreshold = 0.0001f;

			for (size_t i = 0; i < lights_.size() && i < 10; ++i) {
				const auto& l = lights_[i];
				if (!l.casts_shadow)
					continue;

				float weight = l.intensity * haze_density_;
				if (l.type == DIRECTIONAL_LIGHT)
					weight *= 10.0f;

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
