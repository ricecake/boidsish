#include "post_processing/effects/AtmosphereEffect.h"

#include <array>
#include <glm/gtc/matrix_transform.hpp>

#include "atmosphere_manager.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include "light_manager.h"
#include "service_locator.h"
#include "shadow_manager.h"
#include "shader.h"
#include "terrain_render_manager.h"
#include "ConfigManager.h"

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
				glDeleteTextures(1, &bounding_texture_);
				glDeleteTextures(1, &filtered_texture_);
				glDeleteTextures(1, &spatial_aux_texture_);
			}
			if (error_map_texture_) {
				glDeleteTextures(1, &error_map_texture_);
			}
			if (tile_queue_ssbo_) {
				glDeleteBuffers(1, &tile_queue_ssbo_);
			}
			if (indirect_dispatch_ssbo_) {
				glDeleteBuffers(1, &indirect_dispatch_ssbo_);
			}
			if (temporal_textures_[0]) {
				glDeleteTextures(2, temporal_textures_);
				glDeleteTextures(2, temporal_depth_textures_);
				glDeleteTextures(2, temporal_moments_textures_);
			}
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			auto& cfg = ConfigManager::GetInstance();
			cloud_flow_speed_ = cfg.GetAppSettingFloat("cloud_flow_speed", cloud_flow_speed_);
			cloud_flow_direction_ = cfg.GetAppSettingFloat("cloud_flow_direction", cloud_flow_direction_);
			cloud_flow_height_scale_ = cfg.GetAppSettingFloat("cloud_flow_height_scale", cloud_flow_height_scale_);
			cloud_curl_strength_ = cfg.GetAppSettingFloat("cloud_curl_strength", cloud_curl_strength_);
			cloud_curl_frequency_ = cfg.GetAppSettingFloat("cloud_curl_frequency", cloud_curl_frequency_);

			cloud_spatial_update_frames_ = cfg.GetAppSettingInt("cloud_spatial_update_frames", cloud_spatial_update_frames_);
			cloud_max_refresh_rate_ = std::clamp(cfg.GetAppSettingFloat("cloud_max_refresh_rate", cloud_max_refresh_rate_), 0.01f, 1.0f);
			cloud_priority_error_weight_ = cfg.GetAppSettingFloat("cloud_priority_error_weight", cloud_priority_error_weight_);
			cloud_priority_grad_weight_ = cfg.GetAppSettingFloat("cloud_priority_grad_weight", cloud_priority_grad_weight_);
			cloud_priority_age_weight_ = cfg.GetAppSettingFloat("cloud_priority_age_weight", cloud_priority_age_weight_);
			cloud_priority_neighbor_error_weight_ = cfg.GetAppSettingFloat("cloud_priority_neighbor_error_weight", cloud_priority_neighbor_error_weight_);
			cloud_priority_neighbor_grad_weight_ = cfg.GetAppSettingFloat("cloud_priority_neighbor_grad_weight", cloud_priority_neighbor_grad_weight_);
			cloud_priority_threshold_ = cfg.GetAppSettingFloat("cloud_priority_threshold", cloud_priority_threshold_);

			cloud_phase_g1_ = cfg.GetAppSettingFloat("cloud_phase_g1", cloud_phase_g1_);
			cloud_phase_g2_ = cfg.GetAppSettingFloat("cloud_phase_g2", cloud_phase_g2_);
			cloud_phase_alpha_ = cfg.GetAppSettingFloat("cloud_phase_alpha", cloud_phase_alpha_);
			cloud_phase_isotropic_ = cfg.GetAppSettingFloat("cloud_phase_isotropic", cloud_phase_isotropic_);
			cloud_powder_scale_ = cfg.GetAppSettingFloat("cloud_powder_scale", cloud_powder_scale_);
			cloud_powder_multiplier_ = cfg.GetAppSettingFloat("cloud_powder_multiplier", cloud_powder_multiplier_);
			cloud_powder_local_scale_ = cfg.GetAppSettingFloat("cloud_powder_local_scale", cloud_powder_local_scale_);
			cloud_shadow_optical_depth_multiplier_ = cfg.GetAppSettingFloat("cloud_shadow_optical_depth_multiplier", cloud_shadow_optical_depth_multiplier_);
			cloud_shadow_step_multiplier_ = cfg.GetAppSettingFloat("cloud_shadow_step_multiplier", cloud_shadow_step_multiplier_);
			cloud_sun_light_scale_ = cfg.GetAppSettingFloat("cloud_sun_light_scale", cloud_sun_light_scale_);
			cloud_moon_light_scale_ = cfg.GetAppSettingFloat("cloud_moon_light_scale", cloud_moon_light_scale_);
			cloud_beer_powder_mix_ = cfg.GetAppSettingFloat("cloud_beer_powder_mix", cloud_beer_powder_mix_);

			cloud_tile_scheduler_shader_ = std::make_unique<ComputeShader>("shaders/effects/cloud_tile_scheduler.comp");
			cloud_render_shader_ = std::make_unique<ComputeShader>("shaders/effects/atmosphere_lowres.comp");
			cloud_bounding_shader_ = std::make_unique<ComputeShader>("shaders/effects/cloud_bounding.comp");
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
				cloud_render_shader_->trySetInt("u_cloudMinMaxBoundingTexture", Constants::TextureUnit::CloudMinMaxBounding());
				cloud_render_shader_->trySetInt("u_cloudWeatherMinMaxTexture", Constants::TextureUnit::CloudWeatherMinMax());
				cloud_render_shader_->trySetInt("u_cloud3DTexture", Constants::TextureUnit::Cloud3D());
			}

			if (cloud_bounding_shader_ && cloud_bounding_shader_->isValid()) {
				cloud_bounding_shader_->use();
				cloud_bounding_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
				cloud_bounding_shader_->bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
				cloud_bounding_shader_->bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
				cloud_bounding_shader_->bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
				cloud_bounding_shader_->bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());
				cloud_bounding_shader_->setInt("depthTexture", 0);
				cloud_bounding_shader_->trySetInt("u_cloudWeatherMinMaxTexture", Constants::TextureUnit::CloudWeatherMinMax());
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
				glGenTextures(1, &bounding_texture_);
				glGenTextures(1, &filtered_texture_);
				glGenTextures(1, &spatial_aux_texture_);
				glGenTextures(1, &error_map_texture_);
				glGenBuffers(1, &tile_queue_ssbo_);
				glGenBuffers(1, &indirect_dispatch_ssbo_);
			}

			// Allocate cloud raymarching targets at full resolution (persistent between frames)
			glBindTexture(GL_TEXTURE_2D, packed_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindTexture(GL_TEXTURE_2D, packed_depth_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glBindTexture(GL_TEXTURE_2D, packed_velocity_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width_, height_, 0, GL_RG, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			glBindTexture(GL_TEXTURE_2D, bounding_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
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

			// 8x8 Tile Temporal Error Map Texture
			int tile_cols = (width_ + 7) / 8;
			int tile_rows = (height_ + 7) / 8;
			int total_tiles = tile_cols * tile_rows;

			glBindTexture(GL_TEXTURE_2D, error_map_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tile_cols, tile_rows, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindTexture(GL_TEXTURE_2D, 0);

			// Allocate Tile Queue SSBO and Indirect Dispatch Buffer
			struct TileQueueHeader {
				uint32_t count;
				uint32_t max_tiles;
				uint32_t spatial_update_frames;
				uint32_t frame_index;
				float min_refresh_rate;
				float max_refresh_rate;
				uint32_t tile_cols;
				uint32_t tile_rows;
				uint32_t total_tiles;
				uint32_t padding;
			};

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, tile_queue_ssbo_);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TileQueueHeader) + total_tiles * sizeof(glm::uvec2), nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			struct DispatchIndirectCommand {
				uint32_t count_x;
				uint32_t count_y;
				uint32_t count_z;
			};

			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_dispatch_ssbo_);
			glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof(DispatchIndirectCommand), nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
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
			auto& loc = ServiceLocator::Instance();

			GLint original_fbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &original_fbo);

			float dt = time_ - last_time_;
			last_time_ = time_;

			int packed_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int packed_height = std::max(1, static_cast<int>(height_ * render_scale_));

			glm::mat4 invView = glm::inverse(viewMatrix);
			glm::mat4 invProj = glm::inverse(projectionMatrix);

			int tile_cols = (width_ + 7) / 8;
			int tile_rows = (height_ + 7) / 8;
			uint32_t total_tiles = static_cast<uint32_t>(tile_cols * tile_rows);
			uint32_t max_budget_tiles = static_cast<uint32_t>(std::clamp(std::floor(total_tiles * cloud_max_refresh_rate_), 1.0f, static_cast<float>(total_tiles)));

			struct TileQueueHeader {
				uint32_t count;
				uint32_t max_tiles;
				uint32_t spatial_update_frames;
				uint32_t frame_index;
				float min_refresh_rate;
				float max_refresh_rate;
				uint32_t tile_cols;
				uint32_t tile_rows;
				uint32_t total_tiles;
				uint32_t padding;
			};

			TileQueueHeader header{};
			header.count = 0;
			header.max_tiles = max_budget_tiles;
			header.spatial_update_frames = static_cast<uint32_t>(std::max(1, cloud_spatial_update_frames_));
			header.frame_index = static_cast<uint32_t>(frame_index_);
			header.min_refresh_rate = 1.0f / static_cast<float>(header.spatial_update_frames);
			header.max_refresh_rate = cloud_max_refresh_rate_;
			header.tile_cols = static_cast<uint32_t>(tile_cols);
			header.tile_rows = static_cast<uint32_t>(tile_rows);
			header.total_tiles = total_tiles;
			header.padding = 0;

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, tile_queue_ssbo_);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(TileQueueHeader), &header);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			struct DispatchIndirectCommand {
				uint32_t count_x;
				uint32_t count_y;
				uint32_t count_z;
			};

			DispatchIndirectCommand indirect_cmd{0, 1, 1};
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_dispatch_ssbo_);
			glBufferSubData(GL_DISPATCH_INDIRECT_BUFFER, 0, sizeof(DispatchIndirectCommand), &indirect_cmd);
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

			// --- PASS 0: Hierarchical DDA Bounding pass ---
			if (cloud_bounding_shader_ && cloud_bounding_shader_->isValid()) {
				cloud_bounding_shader_->use();
				cloud_bounding_shader_->setFloat("uCloudMaxRayDistance", cloud_max_ray_distance_);
				cloud_bounding_shader_->setFloat("uRenderScale", render_scale_);
				cloud_bounding_shader_->setInt("uFrameIndex", frame_index_);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherMinMax());
				auto atm_mgr = ServiceLocator::Instance().Get<AtmosphereManager>();
				if (atm_mgr) {
					glBindTexture(GL_TEXTURE_2D, atm_mgr->GetCloudWeatherMinMaxTexture());
				}

				glBindImageTexture(0, bounding_texture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

				glDispatchCompute((packed_width + 7) / 8, (packed_height + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
			}

			// Setup common uniforms & bindings helper for cloud raymarching pass
			auto run_cloud_render_pass = [&]() {
				if (cloud_render_shader_ && cloud_render_shader_->isValid()) {
					cloud_render_shader_->use();
					cloud_render_shader_->setFloat("uDeltaTime", dt);
					cloud_render_shader_->setVec3("cloudColorUniform", cloud_color_);
					cloud_render_shader_->setFloat("u_atmosphereHeight", atmosphere_height_);

					cloud_render_shader_->setFloat("uCloudMaxRayDistance", cloud_max_ray_distance_);
					cloud_render_shader_->setFloat("uRenderScale", render_scale_);
					cloud_render_shader_->setInt("uCloudMinSamples", cloud_min_samples_);
					cloud_render_shader_->setInt("uCloudMaxSamples", cloud_max_samples_);
					cloud_render_shader_->setFloat("uCloudExtinction", cloud_extinction_);
					cloud_render_shader_->setVec3("uCloudExtinctionColor", cloud_extinction_color_);
					cloud_render_shader_->setVec3("uCloudAlbedo", cloud_albedo_);

					cloud_render_shader_->setInt("depthTexture", 0);
					cloud_render_shader_->setInt("uHistoryDepth", 1);
					cloud_render_shader_->setInt("uHistoryMoments", 2);
					cloud_render_shader_->setInt("u_cloudMinMaxBoundingTexture", 3);
					cloud_render_shader_->setMat4("uPrevViewProjection", prev_view_projection_);
					cloud_render_shader_->setBool("uHasHistory", has_valid_history_);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, depthTexture);
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, temporal_depth_textures_[temporal_index_]);
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, temporal_moments_textures_[temporal_index_]);
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, bounding_texture_);

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
					glBindImageTexture(3, error_map_texture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudTileQueue(), tile_queue_ssbo_);

					glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_dispatch_ssbo_);
					glDispatchComputeIndirect(0);
					glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
				}
			};

			// Helper to setup tile scheduler shader uniforms
			auto setup_scheduler_shader = [&]() {
				if (cloud_tile_scheduler_shader_ && cloud_tile_scheduler_shader_->isValid()) {
					cloud_tile_scheduler_shader_->use();
					cloud_tile_scheduler_shader_->setInt("uBoundingMap", 0);
					cloud_tile_scheduler_shader_->setFloat("uPriorityErrorWeight", cloud_priority_error_weight_);
					cloud_tile_scheduler_shader_->setFloat("uPriorityGradWeight", cloud_priority_grad_weight_);
					cloud_tile_scheduler_shader_->setFloat("uPriorityAgeWeight", cloud_priority_age_weight_);
					cloud_tile_scheduler_shader_->setFloat("uPriorityNeighborErrorWeight", cloud_priority_neighbor_error_weight_);
					cloud_tile_scheduler_shader_->setFloat("uPriorityNeighborGradWeight", cloud_priority_neighbor_grad_weight_);
					cloud_tile_scheduler_shader_->setFloat("uPriorityThreshold", cloud_priority_threshold_);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, bounding_texture_);

					glBindImageTexture(0, error_map_texture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudTileQueue(), tile_queue_ssbo_);
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudIndirectDispatch(), indirect_dispatch_ssbo_);
				}
			};

			// Helper to reset SSBO tile counts
			auto reset_tile_queue_counts = [&]() {
				uint32_t zero_count = 0;
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, tile_queue_ssbo_);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero_count);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

				DispatchIndirectCommand indirect_cmd_reset{0, 1, 1};
				glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_dispatch_ssbo_);
				glBufferSubData(GL_DISPATCH_INDIRECT_BUFFER, 0, sizeof(DispatchIndirectCommand), &indirect_cmd_reset);
				glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
			};

			int next_temporal = 1 - temporal_index_;

			// Helper to bind image targets & textures and run temporal reprojection pass
			auto run_temporal_pass = [&](bool use_tile_queue) {
				if (temporal_shader_ && temporal_shader_->isValid()) {
					temporal_shader_->use();
					temporal_shader_->setMat4("invProjection", invProj);
					temporal_shader_->setMat4("uPrevViewProjection", prev_view_projection_);
					temporal_shader_->setMat4("invView", invView);
					temporal_shader_->setVec3("viewPos", cameraPos);

					temporal_shader_->setFloat("uDeltaTime", dt);
					temporal_shader_->setBool("uEnableTemporal", enable_temporal_);
					temporal_shader_->setFloat("uRenderScale", render_scale_);
					temporal_shader_->setFloat("uCloudTemporalGamma", cloud_temporal_gamma_);
					temporal_shader_->setFloat("uCloudMaxHistoryLength", cloud_max_history_length_);
					temporal_shader_->setFloat("uCloudMaxRayDistance", cloud_max_ray_distance_);
					temporal_shader_->setBool("uHasHistory", has_valid_history_);
					temporal_shader_->setBool("uUseTileQueue", use_tile_queue);

					temporal_shader_->setInt("uPackedFrame", 0);
					temporal_shader_->setInt("uPackedDepth", 1);
					temporal_shader_->setInt("uPackedVelocity", 2);
					temporal_shader_->setInt("uHistoryFrame", 3);
					temporal_shader_->setInt("uSceneDepth", 4);
					temporal_shader_->setInt("uHistoryCloudDepth", 5);
					temporal_shader_->setInt("uHistoryMoments", 6);
					temporal_shader_->setInt("uBoundingMap", 7);

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
					glActiveTexture(GL_TEXTURE7);
					glBindTexture(GL_TEXTURE_2D, bounding_texture_);

					glBindImageTexture(0, temporal_textures_[next_temporal], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
					glBindImageTexture(1, temporal_depth_textures_[next_temporal], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					glBindImageTexture(2, temporal_moments_textures_[next_temporal], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					glBindImageTexture(3, error_map_texture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudTileQueue(), tile_queue_ssbo_);

					if (use_tile_queue) {
						glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_dispatch_ssbo_);
						glDispatchComputeIndirect(0);
						glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
					} else {
						glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
					}
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
				}
			};

			// --- STEP 1: Spatial Pass Tile Scheduling (Pass 0) ---
			reset_tile_queue_counts();
			if (cloud_tile_scheduler_shader_ && cloud_tile_scheduler_shader_->isValid()) {
				setup_scheduler_shader();
				cloud_tile_scheduler_shader_->setInt("uPass", 0);
				glDispatchCompute((tile_cols + 7) / 8, (tile_rows + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// --- STEP 2: Render Spatial Cloud Tiles ---
			run_cloud_render_pass();

			// --- STEP 3: Full-Resolution Temporal Reprojection (TAA Pass) ---
			run_temporal_pass(false);

			// --- STEP 4: Priority Pass Tile Scheduling (Pass 1 - Evaluates fresh post-TAA error & disocclusion) ---
			reset_tile_queue_counts();
			if (cloud_tile_scheduler_shader_ && cloud_tile_scheduler_shader_->isValid()) {
				setup_scheduler_shader();
				cloud_tile_scheduler_shader_->setInt("uPass", 1);
				glDispatchCompute((tile_cols + 7) / 8, (tile_rows + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// --- STEP 5: Render Priority Cloud Tiles ---
			run_cloud_render_pass();

			// --- STEP 6: Patch Priority Tiles into Temporal Target ---
			run_temporal_pass(true);

			temporal_index_ = next_temporal;
			has_valid_history_ = true;

			// --- PASS 3: Spatial Denoising (Full Resolution) ---
			GLuint cloud_source = temporal_textures_[temporal_index_];
			if (enable_spatial_filter_ && spatial_filter_shader_ && spatial_filter_shader_->isValid()) {
				spatial_filter_shader_->use();
				spatial_filter_shader_->setInt("uCloudColor", 0);
				spatial_filter_shader_->setInt("uCloudDepth", 1);
				spatial_filter_shader_->setInt("uCloudMoments", 2);
				spatial_filter_shader_->setInt("uErrorMap", 3);

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
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, error_map_texture_);

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

			auto shadow_mgr = loc.Get<ShadowManager>();
			auto terrain_mgr = loc.Get<TerrainRenderManager>();
			auto light_mgr = loc.Get<LightManager>();

			if (shadow_mgr && shadow_mgr->IsInitialized() && light_mgr) {
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
			has_valid_history_ = true;
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
