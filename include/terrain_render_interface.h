#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

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
		 * @param world_scale Global world scale factor
		 */
		virtual void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f) = 0;

		/**
		 * @brief Render all visible terrain.
		 * @param is_shadow_pass true if rendering to shadow map
		 */
		virtual void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier,
			bool                            is_shadow_pass = false
		) = 0;

		/**
		 * @brief Commit any pending updates.
		 *
		 * For implementations that batch updates, call once per frame.
		 */
		virtual void CommitUpdates() {}

		/**
		 * @brief Set a callback to be notified when a chunk is evicted due to LRU.
		 */
		virtual void SetEvictionCallback(std::function<void(std::pair<int, int>)> /*callback*/) {}

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
		 * @brief Get the heightmap texture array for shader binding.
		 * Returns 0 if not supported by the implementation.
		 */
		virtual GLuint GetHeightmapTexture() const { return 0; }

		/**
		 * @brief Get info about all registered chunks for external use (e.g., decor placement).
		 * Returns a vector of (world_offset_x, world_offset_z, chunk_data, chunk_size).
		 */
		virtual std::vector<glm::vec4> GetChunkInfo() const { return {}; }
	};

	/**
	 * @brief Templated interface for terrain rendering backends.
	 *
	 * Allows for specialized data types to be passed from the generator
	 * to the renderer while enforcing type safety at the implementation level.
	 */
	template <typename T>
	class ITerrainRenderManagerT: public ITerrainRenderManager {
	public:
		virtual ~ITerrainRenderManagerT() = default;

		/**
		 * @brief Register a terrain chunk for rendering with implementation-specific data.
		 *
		 * @param chunk_key Unique identifier (chunk_x, chunk_z)
		 * @param chunk_data The data produced by the matching generator
		 */
		virtual void RegisterChunk(std::pair<int, int> chunk_key, const T& chunk_data) = 0;
	};

} // namespace Boidsish
