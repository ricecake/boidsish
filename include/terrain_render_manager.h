#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <optional>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	class Terrain;

	/**
	 * @brief Manages batched terrain rendering for improved performance.
	 *
	 * Instead of each terrain chunk having its own VAO/VBO and being rendered
	 * with separate draw calls, this manager consolidates all terrain data into
	 * large persistent buffers and renders everything in a single draw call.
	 *
	 * Data layout:
	 * - Vertex buffer: interleaved [position(3) + normal(3) + texcoord(2)] = 8 floats per vertex
	 * - Index buffer: unsigned int indices with baseVertex offset per chunk
	 *
	 * When chunks are added/removed, only the affected regions of the buffers are updated.
	 */
	class TerrainRenderManager {
	public:
		TerrainRenderManager();
		~TerrainRenderManager();

		// Non-copyable
		TerrainRenderManager(const TerrainRenderManager&) = delete;
		TerrainRenderManager& operator=(const TerrainRenderManager&) = delete;

		/**
		 * @brief Register a terrain chunk for batched rendering.
		 *
		 * @param chunk_key Unique identifier for this chunk (e.g., {chunk_x, chunk_z})
		 * @param vertices Interleaved vertex data (position + normal + texcoord)
		 * @param indices Index data for this chunk
		 * @param world_offset World position offset for this chunk
		 */
		void RegisterChunk(
			std::pair<int, int>               chunk_key,
			const std::vector<float>&         vertices,
			const std::vector<unsigned int>&  indices,
			const glm::vec3&                  world_offset
		);

		/**
		 * @brief Unregister a terrain chunk, freeing its buffer space.
		 *
		 * @param chunk_key The chunk identifier to remove
		 */
		void UnregisterChunk(std::pair<int, int> chunk_key);

		/**
		 * @brief Check if a chunk is registered.
		 */
		bool HasChunk(std::pair<int, int> chunk_key) const;

		/**
		 * @brief Render all registered terrain chunks in a single draw call.
		 *
		 * @param shader The terrain shader to use
		 * @param view The view matrix
		 * @param projection The projection matrix
		 * @param clip_plane Optional clip plane for reflection rendering
		 * @param tess_quality_multiplier Tessellation quality multiplier
		 */
		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		);

		/**
		 * @brief Commit any pending buffer updates to the GPU.
		 *
		 * Call this once per frame after all chunk registrations/unregistrations.
		 */
		void CommitUpdates();

		/**
		 * @brief Get statistics for debugging/profiling.
		 */
		size_t GetRegisteredChunkCount() const;
		size_t GetTotalVertexCount() const;
		size_t GetTotalIndexCount() const;

	private:
		struct ChunkAllocation {
			size_t    vertex_offset;   // Offset in vertices (not bytes)
			size_t    vertex_count;
			size_t    index_offset;    // Offset in indices (not bytes)
			size_t    index_count;
			glm::vec3 world_offset;
		};

		// Buffer management
		void EnsureBufferCapacity(size_t required_vertices, size_t required_indices);
		void RebuildBuffers();
		void UploadChunkData(
			const ChunkAllocation&           allocation,
			const std::vector<float>&        vertices,
			const std::vector<unsigned int>& indices
		);

		// OpenGL resources
		GLuint vao_ = 0;
		GLuint vbo_ = 0;
		GLuint ebo_ = 0;

		// Buffer capacities (in elements, not bytes)
		size_t vertex_capacity_ = 0;
		size_t index_capacity_ = 0;

		// Current usage
		size_t vertex_usage_ = 0;
		size_t index_usage_ = 0;

		// Chunk allocations
		std::map<std::pair<int, int>, ChunkAllocation> chunk_allocations_;

		// Free list for reusing deallocated space
		struct FreeBlock {
			size_t offset;
			size_t size;
		};
		std::vector<FreeBlock> vertex_free_list_;
		std::vector<FreeBlock> index_free_list_;

		// Pending updates
		bool needs_rebuild_ = false;
		std::vector<std::pair<int, int>> pending_registrations_;
		std::map<std::pair<int, int>, std::tuple<std::vector<float>, std::vector<unsigned int>, glm::vec3>>
			pending_chunk_data_;

		// Thread safety
		mutable std::mutex mutex_;

		// Draw command for multi-draw
		struct DrawCommand {
			GLuint count;
			GLuint instanceCount;
			GLuint firstIndex;
			GLint  baseVertex;
			GLuint baseInstance;
		};
		std::vector<DrawCommand> draw_commands_;
		GLuint                   draw_command_buffer_ = 0;
		bool                     draw_commands_dirty_ = true;

		// Constants
		static constexpr size_t FLOATS_PER_VERTEX = 8;  // pos(3) + normal(3) + texcoord(2)
		static constexpr size_t INITIAL_VERTEX_CAPACITY = 1024 * 1024;  // 1M vertices
		static constexpr size_t INITIAL_INDEX_CAPACITY = 4 * 1024 * 1024;  // 4M indices
		static constexpr float  GROWTH_FACTOR = 1.5f;
	};

} // namespace Boidsish
