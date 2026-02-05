#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	struct Frustum;

	struct OccluderQuad {
		glm::vec3 corners[4];
	};

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
		 * @brief Render simplified occlusion quads and issue queries.
		 *
		 * Used for hardware occlusion culling.
		 */
		virtual void RenderOccluders(
			Shader&          shader,
			const glm::mat4& view,
			const glm::mat4& projection,
			const Frustum&   frustum
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
	};

} // namespace Boidsish
