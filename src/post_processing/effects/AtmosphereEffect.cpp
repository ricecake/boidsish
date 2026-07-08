#include "post_processing/effects/AtmosphereEffect.h"

#include <array>

#include "atmosphere_manager.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include "light_manager.h"
#include "service_locator.h"
#include "shadow_manager.h"
#include "shader.h"
#include "terrain_render_manager.h"

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {
			if (packed_texture_) {
				glDeleteTextures(1, &packed_texture_);
				glDeleteTextures(1, &packed_depth_texture_);
				glDeleteTextures(1, &packed_velocity_texture_);
				glDeleteTextures(1, &filtered_texture_);
				glDeleteTextures(1, &spatial_aux_texture_);
			}
			if (temporal_textures_[0]) {
				glDeleteTextures(2, temporal_textures_);
				glDeleteTextures(2, temporal_depth_textures_);
				glDeleteTextures(2, temporal_moments_textures_);
			}
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			cloud_render_shader_ = std::make_unique<ComputeShader>("shaders/effects/atmosphere_lowres.comp");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/atmosphere_composite.frag"
			);
			temporal_shader_ = std::make_unique<ComputeShader>("shaders/effects/cloud_temporal_reprojection.comp");
			spatial_filter_shader_ = std::make_unique<ComputeShader>("shaders/effects/cloud_spatial_filter.comp");

			if (cloud_render_shader_ && cloud_render_shader_->isValid()) {
				cloud_render_shader_->use();
				cloud_render_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
				cloud_render_shader_->bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
				cloud_render_shader_->bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
				cloud_render_shader_->bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
				cloud_render_shader_->bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());
				cloud_render_shader_->setInt("shadowMaps", Constants::TextureUnit::ShadowMaps());
				cloud_render_shader_->setInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());
				cloud_render_shader_->setInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());
				cloud_render_shader_->trySetInt("u_noiseTexture", Constants::TextureUnit::NoiseSimplex());
				cloud_render_shader_->trySetInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());
				cloud_render_shader_->trySetInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
				cloud_render_shader_->trySetInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
				cloud_render_shader_->trySetInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
			}

			if (temporal_shader_ && temporal_shader_->isValid()) {
				temporal_shader_->use();
				temporal_shader_->bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
			}

			auto setup_shader = [](Shader& s) {
				s.use();
				s.bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
				s.bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
				s.bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
				s.bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
				s.bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());

				// Explicitly set standard sampler bindings
				s.setInt("shadowMaps", Constants::TextureUnit::ShadowMaps());
			};

			setup_shader(*composite_shader_);

			width_ = width;
			height_ = height;

			InitializePackedResources();
			InitializeTemporalResources();
		}

		void AtmosphereEffect::InitializePackedResources() {
			if (packed_texture_ == 0) {
				glGenTextures(1, &packed_texture_);
				glGenTextures(1, &packed_depth_texture_);
				glGenTextures(1, &packed_velocity_texture_);
				glGenTextures(1, &filtered_texture_);
				glGenTextures(1, &spatial_aux_texture_);
			}

			int packed_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int packed_height = std::max(1, static_cast<int>(height_ * render_scale_));

			// Color: Packed Cloud Color (RGBA16F)
			glBindTexture(GL_TEXTURE_2D, packed_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, packed_width, packed_height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Depth: Packed Cloud Depth (RGBA32F)
			glBindTexture(GL_TEXTURE_2D, packed_depth_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, packed_width, packed_height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Velocity: Packed Cloud Velocity (RG16F)
			glBindTexture(GL_TEXTURE_2D, packed_velocity_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, packed_width, packed_height, 0, GL_RG, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			// Full-res filtered textures
			glBindTexture(GL_TEXTURE_2D, filtered_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glBindTexture(GL_TEXTURE_2D, spatial_aux_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void AtmosphereEffect::InitializeTemporalResources() {
			if (temporal_textures_[0]) {
				glDeleteTextures(2, temporal_textures_);
				glDeleteTextures(2, temporal_depth_textures_);
				glDeleteTextures(2, temporal_moments_textures_);
			}
			glGenTextures(2, temporal_textures_);
			glGenTextures(2, temporal_depth_textures_);
			glGenTextures(2, temporal_moments_textures_);
			for (int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, temporal_textures_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glBindTexture(GL_TEXTURE_2D, temporal_moments_textures_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			has_valid_history_ = false;
		}

		static float Halton(int index, int base) {
			float result = 0.0f;
			float f = 1.0f / base;
			int   i = index;
			while (i > 0) {
				result += f * (i % base);
				i = floor(i / base);
				f = f / base;
			}
			return result;
		}

		void AtmosphereEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			GLint original_fbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &original_fbo);

			float dt = time_ - last_time_;
			last_time_ = time_;

			int packed_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int packed_height = std::max(1, static_cast<int>(height_ * render_scale_));

			glm::mat4 invView = glm::inverse(viewMatrix);
			glm::mat4 invProj = glm::inverse(projectionMatrix);

			// --- PASS 1: Packed Quarter-res Cloud Rendering ---
			if (cloud_render_shader_ && cloud_render_shader_->isValid()) {
				cloud_render_shader_->use();
				cloud_render_shader_->setFloat("uDeltaTime", dt);
				cloud_render_shader_->setVec3("cloudColorUniform", cloud_color_);
				cloud_render_shader_->setFloat("u_atmosphereHeight", atmosphere_height_);

				cloud_render_shader_->setFloat("uCloudMaxRayDistance", cloud_max_ray_distance_);
				cloud_render_shader_->setInt("uCloudMinSamples", cloud_min_samples_);
				cloud_render_shader_->setInt("uCloudMaxSamples", cloud_max_samples_);
				cloud_render_shader_->setFloat("uCloudExtinction", cloud_extinction_);

				cloud_render_shader_->setInt("depthTexture", 0);
				cloud_render_shader_->setInt("uHistoryDepth", 1);
				cloud_render_shader_->setInt("uHistoryMoments", 2);
				cloud_render_shader_->setMat4("uPrevViewProjection", prev_view_projection_);
				cloud_render_shader_->setBool("uHasHistory", has_valid_history_);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, depthTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[temporal_index_]);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, temporal_moments_textures_[temporal_index_]);

				GpuResourceRegistry::Instance().BindTextures({
					Constants::TextureUnit::AtmosphereTransmittance(),
					Constants::TextureUnit::AtmosphereSkyView(),
					Constants::TextureUnit::NoiseSimplex(),
					Constants::TextureUnit::NoiseCurl(),
					Constants::TextureUnit::NoiseBlue(),
					Constants::TextureUnit::NoiseExtra(),
					Constants::TextureUnit::CloudWeatherBake()
				});

				auto atm_mgr = ServiceLocator::Instance().Get<AtmosphereManager>();
				if (atm_mgr) {
					atm_mgr->BindToShader(*cloud_render_shader_);
				}

				glBindImageTexture(0, packed_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glBindImageTexture(1, packed_depth_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
				glBindImageTexture(2, packed_velocity_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

				glDispatchCompute((packed_width + 7) / 8, (packed_height + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// --- PASS 2: Temporal Reprojection (Full Resolution) ---
			if (temporal_shader_ && temporal_shader_->isValid()) {
				int next_temporal = 1 - temporal_index_;

				temporal_shader_->use();
				temporal_shader_->setMat4("invProjection", invProj);

				temporal_shader_->setBool("uEnableTemporal", enable_temporal_);
				temporal_shader_->setFloat("uCloudTemporalGamma", cloud_temporal_gamma_);
				temporal_shader_->setFloat("uCloudMaxHistoryLength", cloud_max_history_length_);

				temporal_shader_->setInt("uPackedFrame", 0);
				temporal_shader_->setInt("uPackedDepth", 1);
				temporal_shader_->setInt("uPackedVelocity", 2);
				temporal_shader_->setInt("uHistoryFrame", 3);
				temporal_shader_->setInt("uSceneDepth", 4);
				temporal_shader_->setInt("uHistoryCloudDepth", 5);
				temporal_shader_->setInt("uHistoryMoments", 6);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, packed_texture_);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, packed_depth_texture_);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, packed_velocity_texture_);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, temporal_textures_[temporal_index_]);
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, depthTexture);
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[temporal_index_]);
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, temporal_moments_textures_[temporal_index_]);

				glBindImageTexture(0, temporal_textures_[next_temporal], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glBindImageTexture(1, temporal_depth_textures_[next_temporal], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
				glBindImageTexture(2, temporal_moments_textures_[next_temporal], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

				glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				temporal_index_ = next_temporal;
				has_valid_history_ = true;
			}

			// --- PASS 3: Spatial Denoising (Full Resolution) ---
			GLuint cloud_source = temporal_textures_[temporal_index_];
			if (enable_spatial_filter_ && spatial_filter_shader_ && spatial_filter_shader_->isValid()) {
				spatial_filter_shader_->use();
				spatial_filter_shader_->setInt("uCloudColor", 0);
				spatial_filter_shader_->setInt("uCloudDepth", 1);
				spatial_filter_shader_->setInt("uCloudMoments", 2);

				spatial_filter_shader_->setFloat("uCloudPhiLuma", cloud_phi_luma_);
				spatial_filter_shader_->setFloat("uCloudPhiDepth", cloud_phi_depth_);
				spatial_filter_shader_->setFloat("uCloudPhiDensity", cloud_phi_density_);

				spatial_filter_shader_->setFloat("uCloudSvgfHistoryBoost", cloud_svgf_history_boost_);
				spatial_filter_shader_->setFloat("uCloudSvgfHistoryThreshold", cloud_svgf_history_threshold_);

				GLuint ping = temporal_textures_[temporal_index_];
				GLuint pong = spatial_aux_texture_;

				for (int i = 0; i < cloud_svgf_passes_; ++i) {
					int step_size = 1 << i;
					spatial_filter_shader_->setInt("uStepSize", step_size);
					spatial_filter_shader_->setInt("uPassIndex", i);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ping);
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[temporal_index_]);
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, temporal_moments_textures_[temporal_index_]);

					glBindImageTexture(0, pong, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

					glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					ping = pong;
					pong = (ping == spatial_aux_texture_) ? filtered_texture_ : spatial_aux_texture_;
				}
				cloud_source = ping;
			}

			// --- PASS 4: Final Composition (Full Resolution) ---
			glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
			glViewport(0, 0, width_, height_);
			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("depthTexture", 1);
			composite_shader_->setInt("cloudTexture", 2);
			composite_shader_->setInt("normalTexture", 3);
			composite_shader_->setInt("cloudDepthTexture", 4);
			composite_shader_->setMat4("invView", invView);
			composite_shader_->setMat4("invProjection", invProj);

			composite_shader_->setFloat("hazeDensity", haze_density_);
			composite_shader_->setFloat("hazeHeight", haze_height_);
			composite_shader_->setVec3("hazeColor", haze_color_);
			composite_shader_->setFloat("u_atmosphereHeight", atmosphere_height_);

			auto& loc = ServiceLocator::Instance();
			auto shadow_mgr = loc.Get<ShadowManager>();
			auto terrain_mgr = loc.Get<TerrainRenderManager>();
			auto light_mgr = loc.Get<LightManager>();

			if (shadow_mgr && shadow_mgr->IsInitialized()) {
				shadow_mgr->BindForRendering(*composite_shader_);
				std::array<int, 10> shadow_indices;
				shadow_indices.fill(-1);
				const auto& lights = light_mgr->GetLights();
				for (size_t j = 0; j < lights.size() && j < 10; ++j) {
					shadow_indices[j] = lights[j].shadow_map_index;
				}
				composite_shader_->setIntArray("lightShadowIndices", shadow_indices.data(), 10);
			}

			if (terrain_mgr) {
				terrain_mgr->BindTerrainData(*composite_shader_);
			}

			composite_shader_->setInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());
			composite_shader_->setInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, cloud_source);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[temporal_index_]);

			GpuResourceRegistry::Instance().BindTextures({
				Constants::TextureUnit::AtmosphereTransmittance(),
				Constants::TextureUnit::AtmosphereAerialPerspective()
			});

			// Composition still happens in a full-screen quad fragment shader for simplicity,
			// though it could also be a compute shader.
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Cleanup
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
			glBindTexture(GL_TEXTURE_3D, 0);
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);

			// Store current VP for next frame's reprojection
			prev_view_projection_ = projectionMatrix * viewMatrix;
			frame_index_++;
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializePackedResources();
			InitializeTemporalResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
