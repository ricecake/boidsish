#pragma once

#include <array>
#include <functional>
#include <vector>

#include "frame_data.h"
#include "light.h"
#include "render_shader.h"
#include "shadow_manager.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class DecorManager;
	class LightManager;
	class NoiseManager;
	class Shape;
	class TerrainRenderManager;

	/**
	 * @brief Self-contained shadow rendering pass.
	 *
	 * Owns all shadow scheduling state (cascade debt, round-robin, thresholds)
	 * and executes the two-phase shadow render (decor then shapes).
	 * Dependencies captured at construction; call sites only pass per-frame data.
	 */
	class ShadowRenderPass {
	public:
		// Callback for rendering shapes into a shadow pass. The caller provides
		// this because ExecuteRenderQueue lives on VisualizerImpl with all the
		// MDI infrastructure. The callback receives the light-space matrix for
		// the current shadow map.
		using ShapeRenderCallback = std::function<void(const glm::mat4& light_space_matrix)>;

		ShadowRenderPass(
			ShadowManager&                        shadow_manager,
			LightManager&                         light_manager,
			DecorManager&                         decor_manager,
			NoiseManager&                         noise_manager,
			std::shared_ptr<TerrainRenderManager> terrain_render_manager,
			ShaderHandle                          shadow_shader_handle
		);

		/**
		 * @brief Determine which shadow map cascades need updating this frame.
		 * Call once per frame during the update phase, before Execute().
		 */
		void ScheduleUpdates(const FrameData& frame, const std::vector<std::shared_ptr<Shape>>& shapes);

		/**
		 * @brief Render into shadow maps using the schedule from ScheduleUpdates().
		 * Phase 1: decor. Phase 2: shapes via the provided callback.
		 */
		void Execute(const FrameData& frame, ShapeRenderCallback render_shapes);

		/**
		 * @brief Whether any maps were scheduled for update this frame.
		 */
		bool HasUpdates() const { return !maps_to_update_.empty(); }

		/**
		 * @brief Access the shadow lights for UBO updates after Execute().
		 */
		const std::vector<Light*>& GetShadowLights() const { return shadow_lights_; }

		/**
		 * @brief The computed scene center (used for shadow framing).
		 */
		const glm::vec3& GetSceneCenter() const { return scene_center_; }

	private:
		// Dependencies (references — VisualizerImpl owns lifetimes)
		ShadowManager&                        shadow_manager_;
		LightManager&                         light_manager_;
		DecorManager&                         decor_manager_;
		NoiseManager&                         noise_manager_;
		std::shared_ptr<TerrainRenderManager> terrain_render_manager_;
		ShaderHandle                          shadow_shader_handle_;

		// Persistent state across frames
		struct ShadowMapState {
			float     debt = 0.0f;
			int       last_update_frame = 0;
			bool      needs_update = false;
			glm::vec3 last_pos{0.0f, -1000.0f, 0.0f};
			glm::vec3 last_front{0.0f, 0.0f, -1.0f};
			glm::vec3 last_light_pos{0.0f};
			glm::vec3 last_light_dir{0.0f, -1.0f, 0.0f};
			glm::mat4 last_light_space_matrix{1.0f};
			float     rotation_accumulator = 0.0f;
		};

		std::array<ShadowMapState, ShadowManager::kMaxShadowMaps> shadow_map_states_;
		int                                                       shadow_update_round_robin_ = 0;
		float                                                     shadow_update_distance_threshold_ = 200.0f;
		glm::vec3                                                 last_shadow_camera_front_{0.0f, 0.0f, -1.0f};

		// Per-frame working state (populated by ScheduleUpdates, consumed by Execute)
		struct MapUpdateInfo {
			int    map_index;
			Light* light;
			int    cascade_index;
			float  weight;
		};

		std::vector<MapUpdateInfo> shadow_map_registry_;
		std::vector<int>           maps_to_update_;
		std::vector<Light*>        shadow_lights_;
		bool                       any_shadow_caster_moved_ = false;
		bool                       camera_is_close_to_scene_ = true;
		glm::vec3                  scene_center_{0.0f};
	};

} // namespace Boidsish
