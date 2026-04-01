#pragma once

#include <cstdint>

#include "frustum.h"
#include "graphics.h"
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Per-frame computed data, passed by value through the render pipeline.
	 *
	 * Populated once at the start of each frame via PopulateFrameData().
	 * Consumed by all render helpers and passes as const FrameData&.
	 *
	 * NextFrame() returns a new FrameData pre-populated with only the
	 * temporal carry-forward fields (prev_view_projection, etc.).
	 * Everything else defaults to zero/identity, forcing the caller to
	 * explicitly populate fresh values — stale data can't leak across frames.
	 */
	struct FrameData {
		// --- Computed fresh each frame ---

		glm::mat4 view{1.0f};
		glm::mat4 projection{1.0f};
		glm::mat4 view_projection{1.0f};
		glm::mat4 inv_view{1.0f};

		glm::vec3 camera_pos{0.0f};
		glm::vec3 camera_front{0.0f, 0.0f, -1.0f};
		float     camera_fov{45.0f};

		glm::vec3 scene_center{0.0f};
		Frustum   camera_frustum;
		Frustum   generator_frustum;

		float    simulation_time{0.0f};
		float    simulation_delta_time{0.0f};
		uint64_t frame_count{0};

		float world_scale{1.0f};
		float far_plane{1000.0f};

		int render_width{0};
		int render_height{0};
		int window_width{0};
		int window_height{0};

		bool has_shockwaves{false};
		bool has_terrain{false};

		FrameConfigCache config;

		// --- Temporal carry-forward (previous frame → current frame) ---

		glm::mat4 prev_view_projection{1.0f};

		/**
		 * @brief Create the next frame's FrameData with temporal fields pre-populated.
		 *
		 * Only carry-forward fields are copied; everything else is default-initialized.
		 * This prevents accidental use of stale per-frame data.
		 */
		FrameData NextFrame() const {
			FrameData next;
			next.prev_view_projection = view_projection;
			return next;
		}
	};

} // namespace Boidsish
