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
	 */
	class ITerrainRenderManager {
	public:
		virtual ~ITerrainRenderManager() = default;

		/**
		 * @brief Register a terrain chunk for rendering.
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
		 */
		virtual void RenderOccluders(Shader& shader) = 0;
	};

} // namespace Boidsish
