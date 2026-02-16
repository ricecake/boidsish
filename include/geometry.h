#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#include "render_shader.h"
#include "material.h"
#include "render_context.h"

namespace Boidsish {

	/**
	 * @brief Defines the logical layers for rendering to control draw order.
	 */
	enum class RenderLayer : uint8_t {
		Background = 0,
		Opaque = 1,
		Transparent = 2,
		UI = 3,
		Overlay = 4
	};

	/**
	 * @brief Grouped common uniforms for easier management and use across objects.
	 * Layout matches std430 for use in SSBOs.
	 */
	struct CommonUniforms {
		glm::mat4 model = glm::mat4(1.0f);   // 64 bytes
		glm::vec3 color = glm::vec3(1.0f);   // 12 bytes
		float     alpha = 1.0f;              // 4 bytes -> 16 bytes for color+alpha

		int       use_pbr = 0;               // 4 bytes (bool as int for std430)
		float     roughness = 0.5f;          // 4 bytes
		float     metallic = 0.0f;           // 4 bytes
		float     ao = 1.0f;                 // 4 bytes -> 16 bytes

		int       use_texture = 0;           // 4 bytes
		int       is_line = 0;               // 4 bytes
		int       line_style = 0;            // 4 bytes
		int       is_text_effect = 0;        // 4 bytes -> 16 bytes

		float     text_fade_progress = 1.0f; // 4 bytes
		float     text_fade_softness = 0.1f; // 4 bytes
		int       text_fade_mode = 0;        // 4 bytes
		int       is_arcade_text = 0;        // 4 bytes -> 16 bytes

		int       arcade_wave_mode = 0;      // 4 bytes
		float     arcade_wave_amplitude = 0.5f; // 4 bytes
		float     arcade_wave_frequency = 10.0f;// 4 bytes
		float     arcade_wave_speed = 5.0f;     // 4 bytes -> 16 bytes

		int       arcade_rainbow_enabled = 0;   // 4 bytes
		float     arcade_rainbow_speed = 2.0f;  // 4 bytes
		float     arcade_rainbow_frequency = 5.0f;// 4 bytes
		int       checkpoint_style = 0;         // 4 bytes -> 16 bytes

		float     checkpoint_radius = 0.0f;     // 4 bytes
		float     padding[3] = {0,0,0};         // 12 bytes padding to match 16-byte alignment
	};

	/**
	 * @brief Contains all the data necessary for a single draw call.
	 * This decouples the what-to-render from the how-to-render.
	 */
	struct RenderPacket {
		/**
		 * @brief Packed 64-bit ID for sorting packets to minimize GPU state changes.
		 * Layout (high to low bits):
		 * [Layer: 8] [Shader: 16] [Material: 16] [Depth: 24]
		 *
		 * For opaque: sorting ascending will group by layer, then shader, then material,
		 * and finally by depth (front-to-back).
		 */
		uint64_t sort_key = 0;

		// Resource handles
		ShaderHandle   shader_handle;
		MaterialHandle material_handle;

		// Raw OpenGL resources (kept for compatibility and low-level access)
		unsigned int vao = 0;
		unsigned int vbo = 0;
		unsigned int ebo = 0;
		unsigned int shader_id = 0; // The compiled program ID

		unsigned int vertex_count = 0;
		unsigned int index_count = 0;

		// OpenGL drawing mode (e.g., GL_TRIANGLES)
		unsigned int draw_mode = 0;
		// OpenGL index type (e.g., GL_UNSIGNED_INT)
		unsigned int index_type = 0;

		// Grouped common uniforms
		CommonUniforms uniforms;

		// Texture information
		struct TextureInfo {
			unsigned int id;
			std::string  type;
		};
		std::vector<TextureInfo> textures;

		// Instancing
		bool is_instanced = false;
		int  instance_count = 0;
	};

	/**
	 * @brief Abstract base class for geometric objects that can provide RenderPackets.
	 * The ultimate goal is for geometry to return data needed to render it, and a
	 * different loop actually handles rendering.
	 */
	class Geometry {
	public:
		virtual ~Geometry() = default;

		/**
		 * @brief Generates one or more RenderPackets describing how this geometry should be rendered.
		 * @param out_packets Vector to append the generated packets to.
		 * @param context The current frame's rendering context.
		 */
		virtual void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const = 0;
	};

	/**
	 * @brief Helper to calculate a 64-bit sort key for a RenderPacket.
	 *
	 * @param layer The render layer (highest priority)
	 * @param shader The shader handle
	 * @param material The material handle
	 * @param depth Normalized depth [0.0, 1.0]
	 * @return A packed 64-bit key for sorting
	 */
	inline uint64_t CalculateSortKey(RenderLayer layer, ShaderHandle shader, MaterialHandle material, float depth) {
		uint64_t key = 0;
		// Layer: 8 bits (56-63)
		key |= static_cast<uint64_t>(layer) << 56;
		// Shader: 16 bits (40-55)
		key |= static_cast<uint64_t>(shader.id & 0xFFFF) << 40;
		// Material: 16 bits (24-39)
		key |= static_cast<uint64_t>(material.id & 0xFFFF) << 24;

		// Depth: 24 bits (0-23)
		// Map [0, 1] to [0, 16777215]
		uint32_t d;
		if (layer == RenderLayer::Transparent) {
			// Transparent sorting: Back-to-front (larger depth = smaller value for ascending sort)
			d = static_cast<uint32_t>((1.0f - glm::clamp(depth, 0.0f, 1.0f)) * 16777215.0f);
		} else {
			// Opaque sorting: Front-to-back (smaller depth = smaller value)
			d = static_cast<uint32_t>(glm::clamp(depth, 0.0f, 1.0f) * 16777215.0f);
		}
		key |= (d & 0xFFFFFF);

		return key;
	}

} // namespace Boidsish
