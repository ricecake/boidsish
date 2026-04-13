#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <cstdint>

#include "light.h"
#include "temporal_data.h"
#include "visual_effects.h"

namespace Boidsish {

	/**
	 * @brief Encapsulates all global GPU resource bindings and state for a single frame.
	 * This is passed to managers during their update and render calls to ensure
	 * they use the correct per-frame offsets in persistent buffers.
	 */
	struct GlobalRenderState {
		// Active UBO/SSBO bindings for the current frame
		struct BufferRange {
			GLuint id = 0;
			size_t offset = 0;
			size_t size = 0;
		};

		BufferRange lighting;
		BufferRange temporal;
		BufferRange visual_effects;
		BufferRange frustum;

		// Pointers to the current frame's data in persistent mapped buffers (optional, for CPU access)
		LightingUbo*      lighting_ptr = nullptr;
		TemporalUbo*      temporal_ptr = nullptr;
		VisualEffectsUbo* visual_effects_ptr = nullptr;

		// Common transformation matrices
		glm::mat4 view{1.0f};
		glm::mat4 projection{1.0f};
		glm::mat4 view_projection{1.0f};
		glm::vec3 camera_pos{0.0f};

		float time = 0.0f;
		float delta_time = 0.0f;
		float ambient_particle_density = 0.15f;
		uint64_t frame_index = 0;

		// Helper to bind a UBO range based on this state
		void BindLighting(uint32_t binding_point) const {
			if (lighting.id != 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, lighting.id, lighting.offset, lighting.size);
			}
		}

		void BindTemporal(uint32_t binding_point) const {
			if (temporal.id != 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, temporal.id, temporal.offset, temporal.size);
			}
		}

		void BindVisualEffects(uint32_t binding_point) const {
			if (visual_effects.id != 0) {
				glBindBufferRange(
					GL_UNIFORM_BUFFER,
					binding_point,
					visual_effects.id,
					visual_effects.offset,
					visual_effects.size
				);
			}
		}

		void BindFrustum(uint32_t binding_point) const {
			if (frustum.id != 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, frustum.id, frustum.offset, frustum.size);
			}
		}
	};

} // namespace Boidsish
