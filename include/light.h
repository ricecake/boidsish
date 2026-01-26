#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

namespace Boidsish {
	enum LightType {
		POINT_LIGHT,
		DIRECTIONAL_LIGHT,
		SPOT_LIGHT,
		EMISSIVE_LIGHT, // Glowing object - point light with emissive surface, can cast shadows
		FLASH_LIGHT     // Explosion/flash - very bright, rapid falloff, typically no shadows
	};

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
			return l;
		}

		static Light CreateDirectional(
			const glm::vec3& pos,
			const glm::vec3& dir,
			float            intens,
			const glm::vec3& col,
			bool             shadows = false
		) {
			Light l = Create(pos, intens, col, shadows);
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

		/**
		 * Create an emissive/glowing object light.
		 * This is a point light that also indicates the object itself should glow.
		 * The inner_cutoff is repurposed as the emissive radius (object size).
		 * Can cast shadows like a regular point light.
		 *
		 * @param pos Light/object position
		 * @param intens Light intensity (also affects emissive brightness)
		 * @param col Light/emissive color
		 * @param emissive_radius Size of the glowing object for soft falloff
		 * @param shadows Whether this light casts shadows
		 */
		static Light CreateEmissive(
			const glm::vec3& pos,
			float            intens,
			const glm::vec3& col,
			float            emissive_radius = 1.0f,
			bool             shadows = false
		) {
			Light l = Create(pos, intens, col, shadows);
			l.type = EMISSIVE_LIGHT;
			l.inner_cutoff = emissive_radius; // Repurpose for emissive object radius
			l.outer_cutoff = 0.0f;
			return l;
		}

		/**
		 * Create an explosion/flash light.
		 * Very bright, rapid inverse-square falloff, short-lived.
		 * The inner_cutoff stores the flash radius, outer_cutoff stores falloff exponent.
		 * Typically doesn't cast shadows (too brief and expensive).
		 *
		 * @param pos Flash epicenter
		 * @param intens Flash intensity (can be very high, e.g., 10-100)
		 * @param col Flash color (typically warm white/orange for explosions)
		 * @param radius Effective radius of the flash
		 * @param falloff_exp Falloff exponent (higher = sharper falloff, default 2.0)
		 */
		static Light CreateFlash(
			const glm::vec3& pos,
			float            intens,
			const glm::vec3& col,
			float            radius = 50.0f,
			float            falloff_exp = 2.0f
		) {
			Light l = Create(pos, intens, col, false); // Flashes typically don't cast shadows
			l.type = FLASH_LIGHT;
			l.inner_cutoff = radius;      // Flash radius
			l.outer_cutoff = falloff_exp; // Falloff exponent
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
