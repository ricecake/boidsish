#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "terrain.h"

class Shader;

namespace Boidsish {

	struct Frustum;

	/**
	 * @brief Common interface for terrain rendering backends.
	 *
	 * This provides a unified API for the TerrainGenerator to use,
	 * allowing different rendering implementations to be swapped.
	 */
	class ITerrainRenderManager {
	public:
		virtual ~ITerrainRenderManager() = default;

		/**
		 * @brief Register a terrain chunk for rendering.
		 *
		 * The data format varies by implementation:
		 * - V1 (batched): Uses pre-computed vertex mesh data
		 * - V2 (instanced): Uses heightmap for GPU displacement
		 *
		 * @param chunk_key Unique identifier (chunk_x, chunk_z)
		 * @param positions Position data (chunk_size+1)^2 elements
		 * @param normals Normal vectors (chunk_size+1)^2 elements
		 * @param indices Index data for mesh topology
		 * @param min_y Minimum height in chunk
		 * @param max_y Maximum height in chunk
		 * @param world_offset World position offset for this chunk
		 * @param occluders Optional occluder quads for visibility testing
		 */
		virtual void RegisterChunk(
			std::pair<int, int>              chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset,
			const std::vector<OccluderQuad>& occluders = {}
		) = 0;

		/**
		 * @brief Unregister a terrain chunk.
		 */
		virtual void UnregisterChunk(std::pair<int, int> chunk_key) = 0;

		/**
		 * @brief Check if a chunk is registered.
		 */
		virtual bool HasChunk(std::pair<int, int> chunk_key) const = 0;

		/**
		 * @brief Prepare for rendering (culling, buffer updates, etc.)
		 *
		 * Called once per frame before Render().
		 *
		 * @param frustum View frustum for culling
		 * @param camera_pos Camera position
		 */
		virtual void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos) = 0;

		/**
		 * @brief Render all visible terrain.
		 */
		virtual void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		) = 0;

		/**
		 * @brief Commit any pending updates.
		 *
		 * For implementations that batch updates, call once per frame.
		 */
		virtual void CommitUpdates() {}

		/**
		 * @brief Get debug statistics.
		 */
		virtual size_t GetRegisteredChunkCount() const = 0;
		virtual size_t GetVisibleChunkCount() const = 0;

		/**
		 * @brief Get chunk size.
		 */
		virtual int GetChunkSize() const = 0;

		/**
		 * @brief Render occluder quads for debugging.
		 *
		 * Visualizes the generated occluder geometry as wireframes.
		 *
		 * @param shader Debug shader to use for rendering
		 */
		virtual void RenderOccluders(Shader& shader) = 0;

		/**
		 * @brief Perform hardware occlusion queries for all visible chunks.
		 *
		 * This populates occlusion state based on results from the previous frame
		 * and issues new queries for the current frame.
		 *
		 * @param view View matrix
		 * @param projection Projection matrix
		 * @param frustum Current view frustum
		 * @param shader Debug shader for bounding box rendering
		 */
		virtual void PerformOcclusionQueries(const glm::mat4& view, const glm::mat4& projection, const Frustum& frustum, Shader& shader) = 0;
	};

} // namespace Boidsish
