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

	/**
	 * @brief Multi-level LOD chunk identifier.
	 *
	 * Uniquely identifies a terrain chunk across LOD rings.
	 * Can construct from std::pair<int, int> for backwards compatibility (lod = 0).
	 */
	struct ChunkKey {
		int lod = 0;
		int x = 0;
		int z = 0;

		ChunkKey() = default;
		ChunkKey(int lod_, int x_, int z_) : lod(lod_), x(x_), z(z_) {}
		ChunkKey(int x_, int z_) : lod(0), x(x_), z(z_) {}
		ChunkKey(const std::pair<int, int>& p) : lod(0), x(p.first), z(p.second) {}

		bool operator<(const ChunkKey& o) const {
			if (lod != o.lod) return lod < o.lod;
			if (x != o.x) return x < o.x;
			return z < o.z;
		}
		bool operator==(const ChunkKey& o) const {
			return lod == o.lod && x == o.x && z == o.z;
		}
		bool operator!=(const ChunkKey& o) const {
			return !(*this == o);
		}

		operator std::pair<int, int>() const {
			return {x, z};
		}
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
		 * @param chunk_key Unique identifier across LOD levels
		 * @param positions Position data (chunk_size+1)^2 elements
		 * @param normals Normal vectors (chunk_size+1)^2 elements
		 * @param indices Index data for mesh topology
		 * @param min_y Minimum height in chunk
		 * @param max_y Maximum height in chunk
		 * @param world_offset World position offset for this chunk
		 */
		virtual void RegisterChunk(
			ChunkKey                         chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<glm::vec2>&    biomes,
			const std::vector<float>&        packed_height_normal,
			const std::vector<uint8_t>&      packed_biomes,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset,
			float                            world_scale
		) = 0;

		/**
		 * @brief Unregister a terrain chunk.
		 */
		virtual void UnregisterChunk(ChunkKey chunk_key) = 0;

		/**
		 * @brief Check if a chunk is registered.
		 */
		virtual bool HasChunk(ChunkKey chunk_key) const = 0;

		/**
		 * @brief Prepare for rendering (culling, buffer updates, etc.)
		 *
		 * Called once per frame before Render().
		 *
		 * @param frustum View frustum for culling
		 * @param camera_pos Camera position
		 */
		virtual void PrepareForRender(
			const Frustum&   frustum,
			const glm::vec3& camera_pos,
			float            world_scale = 1.0f,
			GLuint           lighting_ubo = 0,
			GLintptr         lighting_ubo_offset = 0,
			GLsizeiptr       lighting_ubo_size = 0,
			float            day_time = -1.0f
		) = 0;

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
		virtual void CommitUpdates(bool force_sync = false) {}

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
