#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "material.h"
#include "render_context.h"
#include "render_shader.h"
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Standard vertex format used across the rendering system.
	 */
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Color = glm::vec3(1.0f);
	};

	/**
	 * @brief GPU indirect draw command structures.
	 */
	struct DrawElementsIndirectCommand {
		uint32_t count;
		uint32_t instanceCount;
		uint32_t firstIndex;
		int32_t  baseVertex;
		uint32_t baseInstance;
	};

	struct DrawArraysIndirectCommand {
		uint32_t count;
		uint32_t instanceCount;
		uint32_t first;
		uint32_t baseInstance;
	};

	/**
	 * @brief Represents an allocation within a Megabuffer.
	 */
	struct MegabufferAllocation {
		uint32_t base_vertex = 0;
		uint32_t first_index = 0;
		uint32_t vertex_count = 0;
		uint32_t index_count = 0;
		bool     valid = false;
	};

	/**
	 * @brief Forward declaration for Megabuffer.
	 * Managed by the rendering system to minimize draw calls via buffer consolidation.
	 */
	class Megabuffer {
	public:
		virtual ~Megabuffer() = default;

		/**
		 * @brief Allocate space for geometry that persists across frames.
		 */
		virtual MegabufferAllocation AllocateStatic(uint32_t vertex_count, uint32_t index_count) = 0;

		/**
		 * @brief Allocate space for geometry that only lasts for the current frame.
		 */
		virtual MegabufferAllocation AllocateDynamic(uint32_t vertex_count, uint32_t index_count) = 0;

		/**
		 * @brief Upload vertex and index data to an allocation.
		 */
		virtual void Upload(
			const MegabufferAllocation& alloc,
			const Vertex*               vertices,
			uint32_t                    v_count,
			const uint32_t*             indices = nullptr,
			uint32_t                    i_count = 0
		) = 0;

		/**
		 * @brief Get the shared VAO for this megabuffer.
		 */
		virtual uint32_t GetVAO() const = 0;
	};

	/**
	 * @brief Defines the logical layers for rendering to control draw order.
	 */
	enum class RenderLayer : uint8_t { Background = 0, Opaque = 1, Transparent = 2, UI = 3, Overlay = 4 };

	/**
	 * @brief Grouped common uniforms for easier management and use across objects.
	 * Layout matches std430 for use in SSBOs.
	 */
	struct CommonUniforms {
		glm::mat4 model = glm::mat4(1.0f); // 64 bytes
		glm::vec4 color = glm::vec4(1.0f); // 16 bytes (xyz=color, w=alpha)

		// Material/PBR
		int   use_pbr = 0;      // 4 bytes
		float roughness = 0.5f; // 4 bytes
		float metallic = 0.0f;  // 4 bytes
		float ao = 1.0f;        // 4 bytes -> 16 bytes

		// Feature Flags
		int use_texture = 0;    // 4 bytes
		int is_line = 0;        // 4 bytes
		int line_style = 0;     // 4 bytes
		int is_text_effect = 0; // 4 bytes -> 16 bytes

		// Text/Arcade Effects
		float text_fade_progress = 1.0f; // 4 bytes
		float text_fade_softness = 0.1f; // 4 bytes
		int   text_fade_mode = 0;        // 4 bytes
		int   is_arcade_text = 0;        // 4 bytes -> 16 bytes

		int   arcade_wave_mode = 0;          // 4 bytes
		float arcade_wave_amplitude = 0.5f;  // 4 bytes
		float arcade_wave_frequency = 10.0f; // 4 bytes
		float arcade_wave_speed = 5.0f;      // 4 bytes -> 16 bytes

		int   arcade_rainbow_enabled = 0;      // 4 bytes
		float arcade_rainbow_speed = 2.0f;     // 4 bytes
		float arcade_rainbow_frequency = 5.0f; // 4 bytes
		int   checkpoint_style = 0;            // 4 bytes -> 16 bytes

		// Rendering State Flags
		int   is_colossal = 0;          // 4 bytes
		int   use_ssbo_instancing = 0;  // 4 bytes
		int   use_vertex_color = 0;     // 4 bytes
		float checkpoint_radius = 0.0f; // 4 bytes -> 16 bytes

		// Dissolve Effects
		glm::vec3 dissolve_plane_normal = glm::vec3(0, 1, 0); // 12 bytes
		float     dissolve_plane_dist = 0.0f;                 // 4 bytes -> 16 bytes
		int       dissolve_enabled = 0;                       // 4 bytes
		int       no_cull = 0;                                // 4 bytes

		// Padding to 256 bytes for SSBO alignment safety (176 + 80 = 256)
		// Used 24 bytes (6 floats/ints) from padding. 20 - 6 = 14.
		float padding[14];
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

		// Megabuffer support
		unsigned int base_vertex = 0;
		unsigned int first_index = 0;

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

		// Instancing - instance_count used for SSBO-based instancing if needed.
		int instance_count = 0;

		// Pass identification
		bool casts_shadows = true;
	};

	/**
	 * @brief Abstract base class for geometric objects that can provide RenderPackets.
	 * The ultimate goal is for geometry to return data needed to render it, and a
	 * different loop actually handles rendering.
	 *
	 * For long-lived/static geometry, override IsDirty() to return false when
	 * unchanged, and implement GetCachedPackets()/CachePackets(). This avoids
	 * regenerating RenderPackets every frame. Call MarkDirty() when properties
	 * change (position, color, material, scale). See Shape for reference impl.
	 */
	class Geometry {
	public:
		virtual ~Geometry() = default;

		/**
		 * @brief Generates one or more RenderPackets describing how this geometry should be rendered.
		 * @param out_packets Vector to append the generated packets to.
		 * @param context The current frame's rendering context.
		 */
		virtual void
		GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const = 0;

		/**
		 * @brief Returns true if this geometry needs packet regeneration.
		 * Default returns true (always dirty) for backwards compatibility.
		 */
		virtual bool IsDirty() const { return true; }

		/**
		 * @brief Marks this geometry as clean after packet generation.
		 * Called by the render loop after caching packets.
		 */
		virtual void MarkClean() const {}

		/**
		 * @brief Marks this geometry as needing packet regeneration.
		 * Call this when any property affecting rendering changes.
		 */
		virtual void MarkDirty() {}

		/**
		 * @brief Returns cached packets if available, nullptr otherwise.
		 * @return Pointer to cached packets, or nullptr if dirty/not cached.
		 */
		virtual std::vector<RenderPacket>* GetCachedPackets() { return nullptr; }

		/**
		 * @brief Stores generated packets in the cache.
		 * @param packets The packets to cache (moved).
		 */
		virtual void CachePackets(std::vector<RenderPacket>&& packets) { (void)packets; }
	};

	/**
	 * @brief Helper to calculate a 64-bit sort key for a RenderPacket.
	 *
	 * @param layer The render layer (highest priority)
	 * @param shader The shader handle
	 * @param vao The VAO ID (groups by buffer state)
	 * @param draw_mode OpenGL draw mode (e.g. GL_TRIANGLES)
	 * @param is_indexed Whether the call is indexed
	 * @param material The material handle (groups by texture/uniform state)
	 * @param depth Normalized depth [0.0, 1.0]
	 * @return A packed 64-bit key for sorting
	 */
	inline uint64_t CalculateSortKey(
		RenderLayer    layer,
		ShaderHandle   shader,
		uint32_t       vao,
		uint32_t       draw_mode,
		bool           is_indexed,
		MaterialHandle material,
		float          depth,
		bool           no_cull = false
	) {
		uint64_t key = 0;
		// Layer: 8 bits (56-63)
		key |= (static_cast<uint64_t>(layer) & 0xFF) << 56;

		if (layer == RenderLayer::Transparent) {
			// Transparent Key: [Layer: 8] [Depth: 32] [Shader: 12] [Material: 12]
			// Back-to-front (larger depth = smaller value for ascending sort)
			uint32_t d = static_cast<uint32_t>((1.0f - glm::clamp(depth, 0.0f, 1.0f)) * 4294967295.0f);
			key |= (static_cast<uint64_t>(d)) << 24;
			key |= (static_cast<uint64_t>(shader.id) & 0xFFF) << 12;
			key |= (static_cast<uint64_t>(material.id) & 0xFFF);
		} else {
			// Opaque Key: [Layer: 8] [Shader: 16] [VAO: 11] [Draw State: 5] [Material: 16] [Depth: 8]
			// Groups by state first to maximize MDI batching.

			// Shader: 16 bits (40-55)
			key |= (static_cast<uint64_t>(shader.id) & 0xFFFF) << 40;
			// VAO: 11 bits (29-39)
			key |= (static_cast<uint64_t>(vao) & 0x7FF) << 29;

			// Draw State: 5 bits (24-28) - is_indexed (1 bit) + no_cull (1 bit) + draw_mode (3 bits)
			uint8_t draw_state = 0;
			draw_state |= (is_indexed ? 0x10 : 0x0);
			draw_state |= (no_cull ? 0x08 : 0x0);
			draw_state |= (static_cast<uint8_t>(draw_mode) & 0x7);
			key |= (static_cast<uint64_t>(draw_state)) << 24;

			// Material: 16 bits (8-23)
			key |= (static_cast<uint64_t>(material.id) & 0xFFFF) << 8;

			// Depth: 8 bits (0-7)
			uint32_t d = static_cast<uint32_t>(glm::clamp(depth, 0.0f, 1.0f) * 255.0f);
			key |= (d & 0xFF);
		}

		return key;
	}

} // namespace Boidsish
