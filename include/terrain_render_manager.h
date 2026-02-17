#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "terrain_generator_interface.h"
#include "terrain_render_interface.h"

class Shader;

namespace Boidsish {

	struct Frustum;

	/**
	 * @brief High-performance instanced terrain rendering with heightmap lookup.
	 *
	 * Architecture:
	 * - Single flat grid mesh (NxN quads) instanced for all visible chunks
	 * - Heightmap stored in texture array (one slice per chunk)
	 * - Per-instance data: world offset + heightmap slice index + bounds
	 * - CPU frustum culling filters visible chunks before rendering
	 * - Tessellation shader samples heightmap for vertex displacement
	 *
	 * Benefits:
	 * - Single instanced draw call for all terrain
	 * - Efficient frustum culling on CPU before draw
	 * - Minimal vertex buffer (just one flat grid)
	 * - Heightmap data doesn't need mesh-layout ordering
	 * - GPU does displacement, reducing CPU→GPU bandwidth
	 *
	 * Data flow:
	 * 1. TerrainGenerator produces heightmap data per chunk
	 * 2. RegisterChunk() uploads heightmap to texture array slice
	 * 3. Each frame: PrepareForRender() builds visible instance list
	 * 4. Render() issues single instanced draw call
	 * 5. TES shader samples heightmap to displace flat grid vertices
	 */
	class TerrainRenderManager: public ITerrainRenderManagerT<TerrainGenerationResult> {
	public:
		TerrainRenderManager(int chunk_size = 32, int max_chunks = 512);
		virtual ~TerrainRenderManager();

		// Non-copyable
		TerrainRenderManager(const TerrainRenderManager&) = delete;
		TerrainRenderManager& operator=(const TerrainRenderManager&) = delete;

		/**
		 * @brief Register a terrain chunk for rendering.
		 *
		 * Extracts heights from positions and uploads to texture array.
		 */
		void RegisterChunk(std::pair<int, int> chunk_key, const TerrainGenerationResult& result) override;

		/**
		 * @brief Unregister a terrain chunk, freeing its texture slice.
		 */
		void UnregisterChunk(std::pair<int, int> chunk_key) override;

		/**
		 * @brief Check if a chunk is registered.
		 */
		bool HasChunk(std::pair<int, int> chunk_key) const override;

		/**
		 * @brief Perform frustum culling and prepare instance buffer.
		 */
		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f) override;

		/**
		 * @brief Render all visible terrain chunks with single instanced draw.
		 */
		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier,
			bool                            is_shadow_pass = false
		) override;

		/**
		 * @brief Commit any pending updates (no-op for this implementation).
		 */
		void CommitUpdates() override {}

		/**
		 * @brief Set a callback to be notified when a chunk is evicted due to LRU.
		 *
		 * This allows TerrainGenerator to remove the chunk from its cache
		 * so it will be regenerated when needed.
		 */
		void SetEvictionCallback(std::function<void(std::pair<int, int>)> callback) override {
			eviction_callback_ = callback;
		}

		/**
		 * @brief Get statistics.
		 */
		size_t GetRegisteredChunkCount() const override;
		size_t GetVisibleChunkCount() const override;

		int GetChunkSize() const override { return chunk_size_; }

		/**
		 * @brief Get the heightmap texture array for shader binding.
		 */
		GLuint GetHeightmapTexture() const override { return heightmap_texture_; }

		/**
		 * @brief Get info about all registered chunks for external use (e.g., decor placement).
		 * Returns a vector of (world_offset_x, world_offset_z, texture_slice, chunk_size).
		 */
		std::vector<glm::vec4> GetChunkInfo() const override;

	private:
		// Per-chunk metadata (CPU side)
		struct ChunkInfo {
			int       texture_slice; // Index into texture array
			float     min_y;         // For frustum culling
			float     max_y;         // For frustum culling
			glm::vec2 world_offset;  // (chunk_x * chunk_size, chunk_z * chunk_size)
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
		void
		UploadHeightmapSlice(int slice, const std::vector<float>& heightmap, const std::vector<glm::vec3>& normals);

		// Configuration
		int chunk_size_;           // Grid size per chunk (e.g., 32)
		int max_chunks_;           // Maximum chunks in texture array
		int heightmap_resolution_; // (chunk_size + 1) for vertex corners

		// OpenGL resources
		GLuint grid_vao_ = 0;
		GLuint grid_vbo_ = 0;
		GLuint grid_ebo_ = 0;
		GLuint instance_vbo_ = 0;
		GLuint heightmap_texture_ = 0; // GL_TEXTURE_2D_ARRAY

		// Grid mesh data
		size_t grid_index_count_ = 0;

		// Chunk management
		std::map<std::pair<int, int>, ChunkInfo> chunks_;
		std::vector<int>                         free_slices_; // Available texture slices
		int                                      next_slice_ = 0;

		// Per-frame instance data
		std::vector<InstanceData> visible_instances_;
		size_t                    instance_buffer_capacity_ = 0;

		// Camera position for LRU eviction (updated by PrepareForRender)
		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};
		float     last_world_scale_ = 1.0f;

		// Thread safety
		mutable std::mutex mutex_;

		// Eviction callback for notifying TerrainGenerator
		std::function<void(std::pair<int, int>)> eviction_callback_;
	};

} // namespace Boidsish
