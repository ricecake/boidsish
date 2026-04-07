#include "shadow_render_pass.h"

#include <algorithm>
#include <map>

#include "NoiseManager.h"
#include "decor_manager.h"
#include "light_manager.h"
#include "shape.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>

namespace Boidsish {

	ShadowRenderPass::ShadowRenderPass(
		ShadowManager&                        shadow_manager,
		LightManager&                         light_manager,
		DecorManager&                         decor_manager,
		NoiseManager&                         noise_manager,
		std::shared_ptr<TerrainRenderManager> terrain_render_manager,
		ShaderHandle                          shadow_shader_handle
	):
		shadow_manager_(shadow_manager),
		light_manager_(light_manager),
		decor_manager_(decor_manager),
		noise_manager_(noise_manager),
		terrain_render_manager_(std::move(terrain_render_manager)),
		shadow_shader_handle_(shadow_shader_handle) {}

	void ShadowRenderPass::ScheduleUpdates(const FrameData& frame, const std::vector<std::shared_ptr<Shape>>& shapes) {
		// Detect shadow caster movement
		any_shadow_caster_moved_ = false;
		scene_center_ = glm::vec3(0.0f);
		bool has_shapes = !shapes.empty();
		int  shadow_caster_count = 0;

		if (has_shapes) {
			for (const auto& shape : shapes) {
				if (shape->CastsShadows()) {
					scene_center_ += glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ());
					shadow_caster_count++;
					float distance_moved = glm::distance(
						shape->GetLastPosition(),
						glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ())
					);
					if (distance_moved > 0.01f) {
						any_shadow_caster_moved_ = true;
					}
				}
			}
			if (shadow_caster_count > 0) {
				scene_center_ /= static_cast<float>(shadow_caster_count);
			} else {
				scene_center_ = frame.camera_pos;
			}
		}

		float distance_to_scene = has_shapes ? glm::distance(frame.camera_pos, scene_center_) : 0.0f;

		if (frame.has_terrain || distance_to_scene > shadow_update_distance_threshold_) {
			float grid_size = 10.0f;
			scene_center_.x = std::floor(frame.camera_pos.x / grid_size) * grid_size;
			scene_center_.y = std::floor(frame.camera_pos.y / grid_size) * grid_size;
			scene_center_.z = std::floor(frame.camera_pos.z / grid_size) * grid_size;
			camera_is_close_to_scene_ = true;
		} else {
			camera_is_close_to_scene_ = (distance_to_scene < shadow_update_distance_threshold_);
		}

		// Build shadow map registry
		shadow_map_registry_.clear();
		maps_to_update_.clear();
		shadow_lights_.clear();

		auto light_count = light_manager_.GetShadowCastingLightCount();
		if (!shadow_manager_.IsInitialized() || !frame.config.enable_shadows || light_count == 0)
			return;

		shadow_lights_ = light_manager_.GetShadowCastingLights();

		for (auto& light : light_manager_.GetLights()) {
			light.shadow_map_index = -1;
		}

		int next_map_idx = 0;
		for (auto light : shadow_lights_) {
			if (next_map_idx >= ShadowManager::kMaxShadowMaps)
				break;
			light->shadow_map_index = next_map_idx;
			if (light->type == DIRECTIONAL_LIGHT) {
				for (int c = 0; c < ShadowManager::kMaxCascades; ++c) {
					if (next_map_idx < ShadowManager::kMaxShadowMaps) {
						float weight = (c == 0) ? 8.0f : (c == 1) ? 4.0f : (c == 2) ? 2.0f : 1.0f;
						shadow_map_registry_.push_back({next_map_idx++, light, c, weight});
					}
				}
			} else {
				shadow_map_registry_.push_back({next_map_idx++, light, -1, 4.0f});
			}
		}

		// Debt-based cascade scheduling
		float camera_rotation_delta = 1.0f - glm::dot(frame.camera_front, last_shadow_camera_front_);
		bool  significant_rotation = camera_rotation_delta > 0.001f;
		bool  major_rotation = camera_rotation_delta > 0.01f;

		const int max_updates_per_frame = major_rotation ? 4 : (significant_rotation ? 3 : 2);

		for (const auto& info : shadow_map_registry_) {
			auto& state = shadow_map_states_[info.map_index];
			float camera_move_dist = glm::distance(frame.camera_pos, state.last_pos);
			float rotation_change = 1.0f - glm::dot(frame.camera_front, state.last_front);
			bool  light_moved = glm::distance(info.light->position, state.last_light_pos) > 0.1f ||
				glm::distance(info.light->direction, state.last_light_dir) > 0.01f;

			float rotation_sensitivity = (info.cascade_index == 0) ? 50.0f
				: (info.cascade_index == 1)                        ? 100.0f
				: (info.cascade_index == 2)                        ? 200.0f
																   : 400.0f;
			state.rotation_accumulator += rotation_change * rotation_sensitivity;

			float movement_threshold = (info.cascade_index == 0) ? 0.5f
				: (info.cascade_index == 1)                      ? 2.0f
				: (info.cascade_index == 2)                      ? 5.0f
																 : 10.0f;
			float rotation_threshold = (info.cascade_index == 0) ? 1.0f
				: (info.cascade_index == 1)                      ? 0.7f
				: (info.cascade_index == 2)                      ? 0.5f
																 : 0.3f;

			bool needs_movement_update = camera_move_dist > movement_threshold;
			bool needs_rotation_update = state.rotation_accumulator > rotation_threshold;
			bool movement_detected = any_shadow_caster_moved_ || light_moved ||
				(frame.has_terrain && (needs_movement_update || needs_rotation_update));

			if (movement_detected && camera_is_close_to_scene_) {
				float urgency = info.weight;
				if (needs_rotation_update)
					urgency *= (1.5f + info.cascade_index * 0.5f);
				if (any_shadow_caster_moved_)
					urgency *= 1.5f;
				state.debt += urgency;
			} else {
				state.debt += (0.02f + info.cascade_index * 0.005f);
			}
		}

		// Select cascades by debt
		std::vector<std::pair<float, int>> debt_sorted;
		float                              debt_threshold = significant_rotation ? 1.5f : 2.5f;
		for (const auto& info : shadow_map_registry_) {
			auto& state = shadow_map_states_[info.map_index];
			if (state.debt >= debt_threshold)
				debt_sorted.push_back({state.debt, info.map_index});
		}
		std::sort(debt_sorted.begin(), debt_sorted.end(), std::greater<>());

		for (int i = 0; i < std::min((int)debt_sorted.size(), max_updates_per_frame); ++i)
			maps_to_update_.push_back(debt_sorted[i].second);

		// Force all cascades on major rotation
		if (major_rotation && maps_to_update_.size() < shadow_map_registry_.size()) {
			for (const auto& info : shadow_map_registry_) {
				if (std::find(maps_to_update_.begin(), maps_to_update_.end(), info.map_index) == maps_to_update_.end())
					maps_to_update_.push_back(info.map_index);
			}
		}

		// Round-robin background refresh
		if (maps_to_update_.empty() && !shadow_map_registry_.empty()) {
			shadow_update_round_robin_ = (shadow_update_round_robin_ + 1) % shadow_map_registry_.size();
			if (frame.frame_count % 2 == 0) {
				maps_to_update_.push_back(shadow_map_registry_[shadow_update_round_robin_].map_index);
			}
		}
	}

	void ShadowRenderPass::Execute(const FrameData& frame, ShapeRenderCallback render_shapes) {
		if (maps_to_update_.empty())
			return;

		glEnable(GL_DEPTH_TEST);
		shadow_manager_.GetShadowShader().use();
		noise_manager_.BindDefault(*shadow_manager_.GetShadowShaderPtr());

		// Phase 1: Render Decor into shadow maps
		std::map<Light*, std::vector<int>> updates_by_light;
		for (int map_idx : maps_to_update_) {
			updates_by_light[shadow_map_registry_[map_idx].light].push_back(map_idx);
		}

		for (auto& [light, map_indices] : updates_by_light) {
			// Cull decor once per light using the widest cascade
			int   best_map_idx = -1;
			float max_split = -1.0f;
			for (int map_idx : map_indices) {
				int cascade = shadow_map_registry_[map_idx].cascade_index;
				if (cascade == -1) {
					best_map_idx = map_idx;
					break;
				}
				float split = shadow_manager_.GetCascadeSplits()[cascade];
				if (split > max_split) {
					max_split = split;
					best_map_idx = map_idx;
				}
			}

			const auto& best_info = shadow_map_registry_[best_map_idx];
			glm::vec3   light_dir_to_light = (light->type == DIRECTIONAL_LIGHT)
				? glm::normalize(-light->direction)
				: glm::normalize(light->position - scene_center_);

			// decor_manager_.Cull(
			// 	frame.view,
			// 	frame.projection,
			// 	ShadowManager::kShadowMapSize,
			// 	ShadowManager::kShadowMapSize,
			// 	shadow_manager_.GetLightSpaceMatrix(best_info.map_index),
			// 	light_dir_to_light,
			// 	terrain_render_manager_
			// );

			for (int map_idx : map_indices) {
				const auto& info = shadow_map_registry_[map_idx];
				bool        enable_color = (info.light == &light_manager_.GetLights()[0] ||
				                     (light_manager_.GetLights().size() > 1 &&
				                      info.light == &light_manager_.GetLights()[1]));

				shadow_manager_.BeginShadowPass(
					info.map_index,
					*info.light,
					scene_center_,
					500.0f * std::max(1.0f, frame.world_scale),
					info.cascade_index,
					frame.view,
					frame.camera_fov,
					(float)frame.window_width / (float)frame.window_height,
					true,
					enable_color,
					false // Temporal fade - don't clear color
				);

				decor_manager_.Render(
					frame.view,
					frame.projection,
					shadow_manager_.GetLightSpaceMatrix(info.map_index),
					shadow_manager_.GetShadowShaderPtr().get()
				);
				shadow_manager_.EndShadowPass();
			}
		}

		// Phase 2: Render Shapes into shadow maps
		for (int map_idx : maps_to_update_) {
			const auto& info = shadow_map_registry_[map_idx];
			auto&       state = shadow_map_states_[map_idx];

			bool enable_color = (info.light == &light_manager_.GetLights()[0] ||
			                     (light_manager_.GetLights().size() > 1 &&
			                      info.light == &light_manager_.GetLights()[1]));

			shadow_manager_.BeginShadowPass(
				info.map_index,
				*info.light,
				scene_center_,
				500.0f * std::max(1.0f, frame.world_scale),
				info.cascade_index,
				frame.view,
				frame.camera_fov,
				(float)frame.window_width / (float)frame.window_height,
				false, // DO NOT CLEAR — keep decor shadows
				enable_color,
				false // DO NOT CLEAR COLOR
			);

			render_shapes(shadow_manager_.GetLightSpaceMatrix(info.map_index));

			shadow_manager_.EndShadowPass();

			// Update cascade state
			state.debt = 0.0f;
			state.rotation_accumulator = 0.0f;
			state.last_update_frame = static_cast<int>(frame.frame_count);
			state.last_light_space_matrix = shadow_manager_.GetLightSpaceMatrix(info.map_index);
			state.last_pos = frame.camera_pos;
			state.last_front = frame.camera_front;
			state.last_light_pos = info.light->position;
			state.last_light_dir = info.light->direction;
		}

		last_shadow_camera_front_ = frame.camera_front;
		shadow_manager_.UpdateShadowUBO(shadow_lights_);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

} // namespace Boidsish
