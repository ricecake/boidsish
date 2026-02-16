#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <vector>

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
	 * @brief Complete lighting UBO data for single-call upload (std140 layout).
	 * Must match layout in shaders/lighting.glsl.
	 * Replaces 8 separate glBufferSubData calls with one for better GPU throughput.
	 */
	struct alignas(16) LightingUbo {
		LightGPU lights[10];                 // offset 0,   640 bytes
		int      num_lights;                 // offset 640, 4 bytes
		float    world_scale;                // offset 644, 4 bytes
		float    _pad1[2];                   // offset 648, 8 bytes (align vec3 to 16)
		alignas(16) glm::vec3 view_pos;      // offset 656, 12 bytes
		float _pad2;                         // offset 668, 4 bytes
		alignas(16) glm::vec3 ambient_light; // offset 672, 12 bytes
		float time;                          // offset 684, 4 bytes
		alignas(16) glm::vec3 view_dir;      // offset 688, 12 bytes
		float _pad3;                         // offset 700, 4 bytes
	}; // Total: 704 bytes

	static_assert(sizeof(LightingUbo) == 704, "LightingUbo must be 704 bytes for UBO alignment");

	/**
	 * @brief Light source data structure for rendering.
	 */
	enum class LightBehaviorType { NONE, BLINK, PULSE, EASE_IN, EASE_OUT, EASE_IN_OUT, FLICKER, MORSE };

	struct LightBehavior {
		LightBehaviorType type = LightBehaviorType::NONE;
		float             period = 1.0f;
		float             amplitude = 1.0f;
		float             duty_cycle = 0.5f;
		float             flicker_intensity = 0.0f; // 0-5
		std::string       message;
		float             timer = 0.0f;
		bool              loop = true;

		// Internal state
		std::vector<bool> morse_sequence;
		int               morse_index = -1;
	};

	/**
	 * @brief Light source data structure for rendering.
	 */
	struct Light {
		glm::vec3 position;
		float     intensity;
		float     base_intensity; // Original intensity before behaviors
		glm::vec3 color;
		int       type;
		glm::vec3 direction;

		// For directional lights, we use angles instead of position/direction vectors
		float azimuth = 0.0f;    // degrees, 0 is North (+Z), 90 is East (+X)
		float elevation = 45.0f; // degrees, 0 is horizon, 90 is zenith (+Y)

		float inner_cutoff;
		float outer_cutoff;

		// CPU-side shadow configuration (not uploaded to lighting UBO directly)
		bool casts_shadow = false;
		int  shadow_map_index = -1;

		// State tracking for shadow optimization
		glm::vec3 last_position;
		glm::vec3 last_direction;

		// Animation/Behavior state
		LightBehavior behavior;

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

		void UpdateDirectionFromAngles() {
			float rad_azimuth = glm::radians(azimuth);
			float rad_elevation = glm::radians(elevation);

			glm::vec3 sun_pos;
			sun_pos.x = glm::cos(rad_elevation) * glm::sin(rad_azimuth);
			sun_pos.y = glm::sin(rad_elevation);
			sun_pos.z = glm::cos(rad_elevation) * glm::cos(rad_azimuth);

			direction = -glm::normalize(sun_pos);
		}

		static void GetAnglesFromDirection(const glm::vec3& dir, float& azimuth, float& elevation) {
			glm::vec3 d = -glm::normalize(dir);
			elevation = glm::degrees(glm::asin(glm::clamp(d.y, -1.0f, 1.0f)));
			azimuth = glm::degrees(glm::atan(d.x, d.z));
			if (azimuth < 0.0f)
				azimuth += 360.0f;
		}

		void SetBlink(float period, float duty_cycle = 0.5f) {
			behavior.type = LightBehaviorType::BLINK;
			behavior.period = period;
			behavior.duty_cycle = duty_cycle;
		}

		void SetPulse(float period, float amplitude = 1.0f) {
			behavior.type = LightBehaviorType::PULSE;
			behavior.period = period;
			behavior.amplitude = amplitude;
		}

		void SetEaseIn(float duration) {
			behavior.type = LightBehaviorType::EASE_IN;
			behavior.period = duration;
			behavior.timer = 0.0f;
		}

		void SetEaseOut(float duration) {
			behavior.type = LightBehaviorType::EASE_OUT;
			behavior.period = duration;
			behavior.timer = 0.0f;
		}

		void SetEaseInOut(float duration) {
			behavior.type = LightBehaviorType::EASE_IN_OUT;
			behavior.period = duration;
			behavior.timer = 0.0f;
		}

		void SetFlicker(float intensity = 1.0f) {
			behavior.type = LightBehaviorType::FLICKER;
			behavior.flicker_intensity = intensity;
		}

		void SetMorse(const std::string& msg, float unit_time = 0.2f) {
			behavior.type = LightBehaviorType::MORSE;
			behavior.message = msg;
			behavior.period = unit_time;
			behavior.morse_index = -1; // Trigger sequence generation
		}

		// Construct a light with optional shadow casting
		static Light Create(const glm::vec3& pos, float intens, const glm::vec3& col, bool shadows = false) {
			Light l;
			l.position = pos;
			l.intensity = intens;
			l.base_intensity = intens;
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
			l.behavior.type = LightBehaviorType::NONE;
			return l;
		}

		static Light
		CreateDirectional(float azimuth, float elevation, float intens, const glm::vec3& col, bool shadows = false) {
			Light l = Create(glm::vec3(0.0f), intens, col, shadows);
			l.type = DIRECTIONAL_LIGHT;
			l.azimuth = azimuth;
			l.elevation = elevation;
			l.UpdateDirectionFromAngles();
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
