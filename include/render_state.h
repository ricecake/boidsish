#pragma once

#include <cstdint>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "binding_set.h"
#include "light.h"
#include "temporal_data.h"
#include "visual_effects.h"

namespace Boidsish {

	/**
	 * Encapsulates all global GPU resource bindings and state for a single frame.
	 * Passed to managers during update and render calls so they use the correct
	 * per-frame offsets in persistent buffers.
	 *
	 * Separate from FrameData: FrameData holds logical camera/scene info,
	 * GlobalRenderState holds GPU resource handles and bindings.
	 */
	struct GlobalRenderState {
		struct BufferRange {
			GLuint id = 0;
			size_t offset = 0;
			size_t size = 0;
		};

		// Active UBO bindings for the current frame
		BufferRange lighting;
		BufferRange temporal;
		BufferRange visual_effects;
		BufferRange frustum;

		// CPU pointers to the current frame's data in persistent mapped buffers
		LightingUbo*      lighting_ptr = nullptr;
		TemporalUbo*      temporal_ptr = nullptr;
		VisualEffectsUbo* visual_effects_ptr = nullptr;

		// Common transformation matrices
		glm::mat4 view{1.0f};
		glm::mat4 projection{1.0f};
		glm::mat4 view_projection{1.0f};
		glm::vec3 camera_pos{0.0f};

		float    time = 0.0f;
		float    delta_time = 0.0f;
		float    ambient_particle_density = 0.15f;
		uint64_t frame_index = 0;

		// Tracks the frame's global buffer and texture bindings
		BindingSet global_bindings;

		// Helpers to bind a UBO range based on this state
		void BindLighting(uint32_t binding_point) const {
			if (lighting.id != 0) {
				// GPU_RESOURCE: Binding, lighting.id, needs PersistentBuffer integration
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point,
					lighting.id, lighting.offset, lighting.size);
			}
		}

		void BindTemporal(uint32_t binding_point) const {
			if (temporal.id != 0) {
				// GPU_RESOURCE: Binding, temporal.id, needs PersistentBuffer integration
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point,
					temporal.id, temporal.offset, temporal.size);
			}
		}

		void BindVisualEffects(uint32_t binding_point) const {
			if (visual_effects.id != 0) {
				// GPU_RESOURCE: Binding, visual_effects.id, needs PersistentBuffer integration
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point,
					visual_effects.id, visual_effects.offset, visual_effects.size);
			}
		}

		void BindFrustum(uint32_t binding_point) const {
			if (frustum.id != 0) {
				// GPU_RESOURCE: Binding, frustum.id, needs PersistentBuffer integration
				glBindBufferRange(GL_UNIFORM_BUFFER, binding_point,
					frustum.id, frustum.offset, frustum.size);
			}
		}
	};

} // namespace Boidsish
