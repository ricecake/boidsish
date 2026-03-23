#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "geometry.h"

class Shader;
class ShaderBase;
class ComputeShader;

namespace Boidsish {

	struct Frustum;

	/**
	 * @brief High-performance terrain rendering using GPU-generated meshes and indirect drawing.
	 *
	 * Architecture:
	 * - Heightmap and biome data stored in texture arrays (one slice per chunk)
	 * - Compute shader (terrain_mesh_gen.comp) generates a persistent mesh for each slice in a global SSBO
	 * - Compute shader (terrain_cull.comp) performs frustum and Hi-Z occlusion culling on GPU
	 * - Single glMultiDrawElementsIndirect call renders all visible chunks
	 * - Persistent buffers and atomic counters for efficient GPU-driven pipeline
	 *
	 * Benefits:
	 * - Fully GPU-driven culling and rendering
	 * - Visual quality maintained with pre-generated high-res meshes
	 * - Reduced CPU driver overhead via Indirect Drawing
	 * - Persistent mesh storage avoids per-frame regeneration
	 *
	 * Data flow:
	 * 1. TerrainGenerator uploads heightmap/biome data to texture arrays
	 * 2. RegisterChunk() dispatches mesh_gen compute for new/updated slices
	 * 3. Each frame: PrepareForRender() dispatches cull compute to fill indirect buffer
	 * 4. Render() issues single glMultiDrawElementsIndirect call using cull results
	 */
	class TerrainRenderManager {
	public:
		TerrainRenderManager(int chunk_size = 32, int max_chunks = 512);
		~TerrainRenderManager();

		// Non-copyable
		TerrainRenderManager(const TerrainRenderManager&) = delete;
		TerrainRenderManager& operator=(const TerrainRenderManager&) = delete;

		/**
		 * @brief Register a terrain chunk for rendering.
		 *
		 * Extracts heights from positions and uploads to texture array.
		 */
		void RegisterChunk(
			std::pair<int, int>              chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<glm::vec2>&    biomes,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset
		);

		/**
		 * @brief Unregister a terrain chunk, freeing its texture slice.
		 */
		void UnregisterChunk(std::pair<int, int> chunk_key);

		/**
		 * @brief Check if a chunk is registered.
		 */
		bool HasChunk(std::pair<int, int> chunk_key) const;

		/**
		 * @brief Perform frustum culling and prepare instance buffer.
		 */
		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f);

		/**
		 * @brief Render all visible terrain chunks with single instanced draw.
		 */
		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		);

		/**
		 * @brief Commit any pending updates (no-op for this implementation).
		 */
		void CommitUpdates() {}

		/**
		 * @brief Set a callback to be notified when a chunk is evicted due to LRU.
		 *
		 * This allows TerrainGenerator to remove the chunk from its cache
		 * so it will be regenerated when needed.
		 */
		void SetEvictionCallback(std::function<void(std::pair<int, int>)> callback) { eviction_callback_ = callback; }

		/**
		 * @brief Get statistics.
		 */
		size_t GetRegisteredChunkCount() const;
		size_t GetVisibleChunkCount() const;

		int GetChunkSize() const { return chunk_size_; }

		/**
		 * @brief Get the heightmap texture array for shader binding.
		 */
		GLuint GetHeightmapTexture() const { return heightmap_texture_; }

		/**
		 * @brief Get the biome texture array for shader binding.
		 */
		GLuint GetBiomeTexture() const { return biome_texture_; }

		/**
		 * @brief Get info about all registered chunks for external use (e.g., decor placement).
		 * Returns a vector of (world_offset_x, world_offset_z, texture_slice, chunk_size).
		 * @param world_scale The world scale to apply to the chunk size.
		 */
		std::vector<glm::vec4> GetChunkInfo(float world_scale) const;

		/**
		 * @brief Pre-computed chunk data for decor placement.
		 * Avoids repeated key derivation and separate update_count queries.
		 */
		struct DecorChunkData {
			std::pair<int, int> key;          // integer chunk coordinate
			glm::vec2           world_offset; // world-space offset
			float               slice;        // texture array slice
			float               chunk_size;   // world-space chunk size
			uint32_t            update_count; // re-upload counter for deformation detection
		};

		/**
		 * @brief Get all registered chunks with pre-computed keys and update counts.
		 * Single mutex acquisition, no intermediate map construction.
		 */
		std::vector<DecorChunkData> GetDecorChunkData(float world_scale) const;

		/**
		 * @brief Bind terrain data textures and UBO to a shader.
		 */
		void BindTerrainData(ShaderBase& shader_base) const;

		void SetHiZData(GLuint texture, int width, int height, int mips, const glm::mat4& prev_vp) {
			hiz_texture_ = texture;
			hiz_width_ = width;
			hiz_height_ = height;
			hiz_mips_ = mips;
			prev_view_projection_ = prev_vp;
			enable_hiz_ = true;
		}

		void SetHiZEnabled(bool enabled) { enable_hiz_ = enabled; }

		void SetNoise(const GLuint& noise, const GLuint& curl, const GLuint& extra = 0) {
			if (noise != 0) {
				noise_texture_ = noise;
			}

			if (curl != 0) {
				curl_texture_ = curl;
			}
			if (extra != 0)
				extra_noise_texture_ = extra;
		}

	private:
		/**
		 * @brief Update the global chunk grid and max height textures.
		 */
		void UpdateGridTextures(float world_scale);

		/**
		 * @brief Generate mipmaps for the max height grid using MAX reduction.
		 */
		void GenerateMaxHeightMips();

		// Per-chunk metadata (CPU side)
		struct ChunkInfo {
			int       texture_slice;    // Index into texture array
			float     min_y;            // For frustum culling
			float     max_y;            // For frustum culling
			glm::vec2 world_offset;     // (chunk_x * chunk_size, chunk_z * chunk_size)
			uint32_t  update_count = 0; // Incremented each time chunk data is re-uploaded
		};

		// Per-instance data sent to GPU (std140 layout)
		struct alignas(16) InstanceData {
			glm::vec4 world_offset_and_slice; // xyz = world offset, w = texture slice index
			glm::vec4 bounds;                 // xy = min/max Y for this chunk (for shader LOD)
		};

		// Frustum culling helper
		bool IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum, float world_scale) const;

		// Create the flat grid mesh
		void CreateGridMesh();

		// Create/resize the heightmap texture array
		void EnsureTextureCapacity(int required_slices);

		// Upload heightmap data to a texture slice
		void UploadHeightmapSlice(
			int                           slice,
			const std::vector<float>&     heightmap,
			const std::vector<glm::vec3>& normals,
			const std::vector<glm::vec2>& biomes
		);

		// Configuration
		int chunk_size_;           // Grid size per chunk (e.g., 128)
		int max_chunks_;           // Maximum chunks in texture array
		int heightmap_resolution_; // (chunk_size + 1) for vertex corners

		struct TerrainVertex {
			glm::vec4 position; // xyz = world pos, w = pad
			glm::vec4 normal;   // xyz = normal, w = pad
			glm::vec4 biome;    // xy = biome indices/weights, zw = pad
		};

		struct ChunkMetadata {
			glm::vec4 world_offset_and_slice; // xyz = world offset, w = slice index
			glm::vec4 bounds;                 // x = minY, y = maxY, zw = pad
			uint32_t  is_active;              // 1 if chunk is registered, 0 otherwise
			uint32_t  pad[3];
		};

		// OpenGL resources
		GLuint terrain_vao_ = 0;
		GLuint terrain_ebo_ = 0;

		GLuint terrain_vertices_ssbo_ = 0;
		GLuint terrain_metadata_ssbo_ = 0;
		GLuint terrain_indirect_args_ssbo_ = 0;
		GLuint terrain_command_counter_buffer_ = 0;

		GLuint heightmap_texture_ = 0; // GL_TEXTURE_2D_ARRAY (RGBA16F: height, normal.xyz)
		GLuint biome_texture_ = 0;     // GL_TEXTURE_2D_ARRAY (RG8: low_idx, t)
		GLuint noise_texture_ = 0;
		GLuint curl_texture_ = 0;
		GLuint extra_noise_texture_ = 0;
		GLuint biome_ubo_ = 0; // UBO for BiomeShaderProperties

		// Global terrain grid resources
		GLuint chunk_grid_texture_ = 0;      // GL_TEXTURE_2D (R16I: texture_slice index, -1 if none)
		GLuint max_height_grid_texture_ = 0; // GL_TEXTURE_2D (R32F: max_y, mips for hierarchical check)
		GLuint terrain_data_ubo_ = 0;        // UBO for grid parameters

		std::unique_ptr<ComputeShader> grid_mip_shader_;
		std::unique_ptr<ComputeShader> mesh_gen_shader_;
		std::unique_ptr<ComputeShader> cull_shader_;

		// Grid mesh data
		size_t indices_per_chunk_ = 0;
		size_t vertices_per_chunk_ = 0;

		// Chunk management
		std::map<std::pair<int, int>, ChunkInfo> chunks_;
		std::vector<int>                         free_slices_; // Available texture slices
		int                                      next_slice_ = 0;

		// Hi-Z Occlusion Culling state
		GLuint    hiz_texture_ = 0;
		int       hiz_width_ = 0;
		int       hiz_height_ = 0;
		int       hiz_mips_ = 0;
		bool      enable_hiz_ = false;
		glm::mat4 prev_view_projection_{1.0f};

		// Camera position for LRU eviction (updated by PrepareForRender)
		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};
		float     last_world_scale_ = 1.0f;

		// Thread safety
		mutable std::mutex mutex_;

		// Eviction callback for notifying TerrainGenerator
		std::function<void(std::pair<int, int>)> eviction_callback_;
	};

} // namespace Boidsish
