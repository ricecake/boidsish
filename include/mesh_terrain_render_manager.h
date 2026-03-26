#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "terrain_render_interface.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader.h"

namespace Boidsish {

	/**
	 * @brief Terrain renderer that uses standard 3D meshes per chunk.
	 *
	 * Unlike TerrainRenderManager which uses a single flat grid and heightmap displacement,
	 * this renderer handles chunks as arbitrary 3D geometry. This allows for complex
	 * structures like caves, tunnels, and multiple vertical layers.
	 */
	class MeshTerrainRenderManager : public ITerrainRenderManager {
	public:
		MeshTerrainRenderManager(int chunk_size = 32);
		~MeshTerrainRenderManager() override;

		void RegisterChunk(
			std::pair<int, int>              chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<glm::vec2>&    biomes,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset
		) override;

		void UnregisterChunk(std::pair<int, int> chunk_key) override;

		bool HasChunk(std::pair<int, int> chunk_key) const override;

		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f) override;

		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		) override;

		std::shared_ptr<Shader> GetDefaultShader() override { return mesh_shader_; }

		void CommitUpdates() override {}

		size_t GetRegisteredChunkCount() const override;
		size_t GetVisibleChunkCount() const override;

		int GetChunkSize() const override { return chunk_size_; }

	private:
		struct ChunkMesh {
			GLuint vao = 0;
			GLuint vbo = 0;
			GLuint ebo = 0;
			GLsizei index_count = 0;
			glm::vec3 world_offset;
			float min_y, max_y;
			bool visible = false;
		};

		int chunk_size_;
		std::map<std::pair<int, int>, std::unique_ptr<ChunkMesh>> chunks_;
		std::vector<ChunkMesh*> visible_chunks_;

		std::shared_ptr<Shader> mesh_shader_;

		mutable std::mutex mutex_;

		void _DestroyChunk(ChunkMesh& chunk);
	};

} // namespace Boidsish
