#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

namespace Boidsish {
	enum LightType { POINT_LIGHT, DIRECTIONAL_LIGHT, SPOT_LIGHT };

	/**
	 * @brief GPU-compatible light data for UBO upload (std140 layout).
	 *
	 * This struct must match the std140 layout of the 'Light' struct in GLSL.
	 * Total size: 64 bytes.
	 */
	struct LightGPU {
		glm::vec3 position;     // offset 0,  12 bytes
		float     intensity;    // offset 12, 4 bytes
		glm::vec3 color;        // offset 16, 12 bytes
		int       type;         // offset 28, 4 bytes
		glm::vec3 direction;    // offset 32, 12 bytes
		float     inner_cutoff; // offset 44, 4 bytes
		float     outer_cutoff; // offset 48, 4 bytes
		float     _padding[3];  // offset 52, 12 bytes of padding
	}; // Total: 64 bytes

	/**
	 * @brief Light source data structure for rendering.
	 */
	struct Light {
		glm::vec3 position;
		float     intensity;
		glm::vec3 color;
		int       type;
		glm::vec3 direction;
		float     inner_cutoff;
		float     outer_cutoff;

		// CPU-side shadow configuration (not uploaded to lighting UBO directly)
		bool casts_shadow = false;
		int  shadow_map_index = -1;

		// State tracking for shadow optimization
		glm::vec3 last_position;
		glm::vec3 last_direction;

		// Convert to GPU-compatible struct for UBO upload
		LightGPU ToGPU() const {
			LightGPU gpu;
			gpu.position = position;
			gpu.intensity = intensity;
			gpu.color = color;
			gpu.type = type;
			gpu.direction = direction;
			gpu.inner_cutoff = inner_cutoff;
			gpu.outer_cutoff = outer_cutoff;
			return gpu;
		}

		// Construct a light with optional shadow casting
		static Light Create(const glm::vec3& pos, float intens, const glm::vec3& col, bool shadows = false) {
			Light l;
			l.position = pos;
			l.intensity = intens;
			l.color = col;
			l.type = POINT_LIGHT;
			l.direction = glm::vec3(0.0f, -1.0f, 0.0f);
			l.inner_cutoff = glm::cos(glm::radians(12.5f));
			l.outer_cutoff = glm::cos(glm::radians(17.5f));
			l.casts_shadow = shadows;
			l.shadow_map_index = -1;
			// Initialize last state for shadow optimization
			l.last_position = l.position;
			l.last_direction = l.direction;
			return l;
		}

		static Light CreateDirectional(const glm::vec3& dir, float intens, const glm::vec3& col, bool shadows = false) {
			Light l = Create({0, 0, 0}, intens, col, shadows);
			l.type = DIRECTIONAL_LIGHT;
			l.direction = dir;
			return l;
		}

		static Light CreateSpot(
			const glm::vec3& pos,
			const glm::vec3& dir,
			float            intens,
			const glm::vec3& col,
			float            inner_angle,
			float            outer_angle,
			bool             shadows = false
		) {
			Light l = Create(pos, intens, col, shadows);
			l.type = SPOT_LIGHT;
			l.direction = dir;
			l.inner_cutoff = glm::cos(glm::radians(inner_angle));
			l.outer_cutoff = glm::cos(glm::radians(outer_angle));
			return l;
		}
	};

	/**
	 * @brief GPU-side light data for shadow mapping.
	 *
	 * This extended structure includes the light-space matrix for shadow
	 * calculations. It's stored in a separate UBO from the basic lighting data.
	 */
	struct ShadowLightData {
		glm::mat4 light_space_matrix; // View-projection from light's perspective
		glm::vec3 position;
		float     padding1;
		int       shadow_map_index; // Which shadow map texture to sample
		int       padding2, padding3, padding4;
	};

} // namespace Boidsish

#endif // LIGHT_H
