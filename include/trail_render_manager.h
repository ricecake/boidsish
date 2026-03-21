#pragma once

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "constants.h"
#include "geometry.h"
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
	 *
	 * Data layout:
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
		 * @brief Update trail point data.
		 *
		 * @param trail_id The trail to update
		 * @param points Control points (position + color)
		 * @param head Ring buffer head index
		 * @param tail Ring buffer tail index
		 * @param is_full Whether the ring buffer has wrapped
		 */
		void UpdateTrailData(
			int                                                trail_id,
			const std::deque<std::pair<glm::vec3, glm::vec3>>& points,
			size_t                                             head,
			size_t                                             tail,
			bool                                               is_full,
			const glm::vec3&                                   min_bound,
			const glm::vec3&                                   max_bound
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
		 * @brief Generate render packets for all active trails.
		 *
		 * @param out_packets Vector to append packets to
		 * @param context Render context
		 * @param shader_handle Handle to the trail shader
		 */
		void GetRenderPackets(
			std::vector<RenderPacket>& out_packets,
			const RenderContext&       context,
			ShaderHandle               shader_handle
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
			size_t points_offset; // Offset in TrailPoints SSBO
			size_t max_points;    // Maximum points allocated for this trail
			size_t head;          // Ring buffer head
			size_t tail;          // Ring buffer tail
			size_t vertex_offset; // Offset in generated VBO
			bool   is_full;       // Ring buffer full flag

			// Per-trail shader parameters
			bool  iridescent = false;
			bool  rocket_trail = false;
			bool  use_pbr = false;
			float roughness = Constants::Class::Trails::DefaultRoughness();
			float metallic = Constants::Class::Trails::DefaultMetallic();
			float base_thickness = Constants::Class::Trails::BaseThickness();

			glm::vec3 min_bound = glm::vec3(0.0f);
			glm::vec3 max_bound = glm::vec3(0.0f);

			bool needs_upload = false;
		};

		// Buffer management
		void EnsureBufferCapacity(size_t required_points);

		// OpenGL resources
		GLuint vao_ = 0;
		GLuint tess_vbo_ = 0;
		GLuint tess_ebo_ = 0;
		GLuint points_ssbo_ = 0;
		GLuint instances_ssbo_ = 0;
		GLuint spine_ssbo_ = 0;

		// Buffer capacity
		size_t points_capacity_ = 0;
		size_t points_usage_ = 0;

		// Trail allocations
		std::map<int, TrailAllocation> trail_allocations_;

		// Pending point data for upload
		std::map<int, std::vector<float>> pending_point_data_;

		// Free list for reusing deallocated space
		struct FreeBlock {
			size_t offset;
			size_t size;
		};

		std::vector<FreeBlock> free_list_;

		// Free list for vertex blocks
		std::vector<size_t> free_vertex_slots_;
		size_t              next_vertex_slot_ = 0;

		// Thread safety
		mutable std::mutex mutex_;

		// Compute shader for tessellation
		std::unique_ptr<class ComputeShader> tess_shader_;

		// Constants
		static constexpr size_t INITIAL_POINTS_CAPACITY = 100000;
		static constexpr float  GROWTH_FACTOR = Constants::Class::Trails::GrowthFactor();
	};

} // namespace Boidsish
