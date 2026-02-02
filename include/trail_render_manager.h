#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "constants.h"
#include "opengl_helpers.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	class Trail;

	/**
	 * @brief Manages batched trail rendering for improved performance.
	 *
	 * Instead of each trail having its own VAO/VBO and being rendered
	 * with separate draw calls, this manager consolidates all trail data into
	 * large persistent buffers.
	 *
	 * Optimizations provided:
	 * - Single VAO bind per frame (vs N binds before)
	 * - Single VBO with all trail data (better cache locality)
	 * - Single shader.use() call
	 * - Only per-trail uniforms updated per trail
	 *
	 * Note: True single draw call not possible with GL_TRIANGLE_STRIP due to:
	 * 1. Strip continuity between trails would connect unrelated geometry
	 * 2. Per-trail shader parameters (iridescent, rocket trail, etc.)
	 * 3. Progress calculation in shader based on gl_VertexID
	 */

	/**
	 * @brief Vertex data for trails.
	 * Matches the layout expected by the trail shader.
	 */
	struct TrailVertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 color;
	};

	/**
	 * @brief Data layout:
	 * - Vertex buffer: interleaved [position(3) + normal(3) + color(3)] = 9 floats per vertex
	 */
	class TrailRenderManager {
	public:
		TrailRenderManager();
		~TrailRenderManager();

		// Non-copyable
		TrailRenderManager(const TrailRenderManager&) = delete;
		TrailRenderManager& operator=(const TrailRenderManager&) = delete;

		/**
		 * @brief Register a trail for batched rendering.
		 *
		 * @param trail_id Unique identifier for this trail
		 * @param max_vertices Maximum vertices this trail will use
		 * @return true if registration successful
		 */
		bool RegisterTrail(int trail_id, size_t max_vertices);

		/**
		 * @brief Unregister a trail, freeing its buffer space.
		 *
		 * @param trail_id The trail identifier to remove
		 */
		void UnregisterTrail(int trail_id);

		/**
		 * @brief Check if a trail is registered.
		 */
		bool HasTrail(int trail_id) const;

		/**
		 * @brief Update trail vertex data.
		 *
		 * @param trail_id The trail to update
		 * @param vertices Vertex data (position + normal + color per vertex)
		 * @param head Ring buffer head index
		 * @param tail Ring buffer tail index
		 * @param vertex_count Number of active vertices
		 * @param is_full Whether the ring buffer has wrapped
		 */
		void UpdateTrailData(
			int                       trail_id,
			const std::vector<float>& vertices,
			size_t                    head,
			size_t                    tail,
			size_t                    vertex_count,
			bool                      is_full
		);

		/**
		 * @brief Set per-trail rendering parameters.
		 */
		void SetTrailParams(
			int   trail_id,
			bool  iridescent,
			bool  rocket_trail,
			bool  use_pbr,
			float roughness,
			float metallic,
			float base_thickness
		);

		/**
		 * @brief Render all registered trails in a single draw call.
		 *
		 * @param shader The trail shader to use
		 * @param view The view matrix
		 * @param projection The projection matrix
		 * @param clip_plane Optional clip plane for reflection rendering
		 */
		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const std::optional<glm::vec4>& clip_plane
		);

		/**
		 * @brief Commit any pending buffer updates to the GPU.
		 *
		 * Call this once per frame after all trail updates.
		 */
		void CommitUpdates();

		/**
		 * @brief Get statistics for debugging/profiling.
		 */
		size_t GetRegisteredTrailCount() const;
		size_t GetTotalVertexCount() const;

	private:
		struct TrailAllocation {
			size_t vertex_offset; // Offset in vertices (not bytes)
			size_t max_vertices;  // Maximum vertices allocated for this trail
			size_t head;          // Ring buffer head
			size_t tail;          // Ring buffer tail
			size_t vertex_count;  // Current active vertex count
			bool   is_full;       // Ring buffer full flag

			// Per-trail shader parameters
			bool  iridescent = false;
			bool  rocket_trail = false;
			bool  use_pbr = false;
			float roughness = Constants::Class::Trails::DefaultRoughness();
			float metallic = Constants::Class::Trails::DefaultMetallic();
			float base_thickness = Constants::Class::Trails::BaseThickness();

			bool needs_upload = false;
		};

		// Buffer management
		void EnsureBufferCapacity(size_t required_vertices);

		// OpenGL resources
		GLuint vao_ = 0;
		std::unique_ptr<PersistentRingBuffer<TrailVertex>> vbo_ring_;

		// Buffer capacity (in vertices, not bytes)
		size_t vertex_capacity_ = 0;
		size_t vertex_usage_ = 0;

		// Trail allocations
		std::map<int, TrailAllocation> trail_allocations_;

		// Pending vertex data for upload
		std::map<int, std::vector<float>> pending_vertex_data_;

		// Free list for reusing deallocated space
		struct FreeBlock {
			size_t offset;
			size_t size;
		};

		std::vector<FreeBlock> free_list_;

		// Draw commands tracking
		bool                                   draw_commands_dirty_ = true;

		// Thread safety
		mutable std::mutex mutex_;

		// Constants
		static constexpr size_t FLOATS_PER_VERTEX =
			Constants::Class::Trails::FloatsPerVertex(); // pos(3) + normal(3) + color(3)
		static constexpr size_t INITIAL_VERTEX_CAPACITY =
			Constants::Class::Trails::InitialVertexCapacity(); // 500k vertices
		static constexpr float GROWTH_FACTOR = Constants::Class::Trails::GrowthFactor();
	};

} // namespace Boidsish
