#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief GPU-compatible light data for UBO upload (std140 layout).
	 *
	 * This struct is EXACTLY 32 bytes and matches the GLSL Light struct.
	 * Use this for uploading to the lighting UBO.
	 */
	struct LightGPU {
		glm::vec3 position;  // offset 0, 12 bytes
		float     intensity; // offset 12, 4 bytes
		glm::vec3 color;     // offset 16, 12 bytes
		float     padding;   // offset 28, 4 bytes (std140 alignment)
	}; // Total: 32 bytes

	/**
	 * @brief Light source data structure for rendering.
	 *
	 * Contains both GPU-compatible data and CPU-only shadow configuration.
	 * Use ToGPU() to get the uploadable portion.
	 */
	struct Light {
		glm::vec3 position;
		float     intensity;
		glm::vec3 color;
		float     padding; // for std140 alignment

		// CPU-side shadow configuration (not uploaded to lighting UBO directly)
		bool casts_shadow = false;
		int  shadow_map_index = -1; // Index into the shadow map array, -1 if no shadow

		// Convert to GPU-compatible struct for UBO upload
		LightGPU ToGPU() const { return LightGPU{position, intensity, color, padding}; }

		// Construct a light with optional shadow casting
		static Light Create(const glm::vec3& pos, float intens, const glm::vec3& col, bool shadows = false) {
			Light l;
			l.position = pos;
			l.intensity = intens;
			l.color = col;
			l.padding = 0.0f;
			l.casts_shadow = shadows;
			l.shadow_map_index = -1;
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
