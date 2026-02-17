#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "terrain_generator_interface.h"
#include "terrain_render_interface.h"

namespace Boidsish {

	/**
	 * @brief Voxel-based terrain renderer.
	 *
	 * Operates by interpreting the heightmap grid as a collection of voxel columns
	 * and generating a blocky mesh from it.
	 */
	class VoxelTerrainRenderManager : public ITerrainRenderManagerT<TerrainGenerationResult> {
	public:
		VoxelTerrainRenderManager(int chunk_size = 32);
		virtual ~VoxelTerrainRenderManager();

		void RegisterChunk(std::pair<int, int> chunk_key, const TerrainGenerationResult& result) override;

		void UnregisterChunk(std::pair<int, int> chunk_key) override;
		bool HasChunk(std::pair<int, int> chunk_key) const override;

		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f) override;

		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier,
			bool                            is_shadow_pass = false
		) override;

		size_t GetRegisteredChunkCount() const override;
		size_t GetVisibleChunkCount() const override;

		GLuint GetHeightmapTexture() const override { return heightmap_texture_; }
		std::vector<glm::vec4> GetChunkInfo() const override;
		int    GetChunkSize() const override { return chunk_size_; }

	private:
		struct ChunkMesh {
			GLuint vao = 0;
			GLuint vbo = 0;
			GLuint ebo = 0;
			size_t index_count = 0;
			glm::vec3 min_corner;
			glm::vec3 max_corner;
			glm::vec3 world_offset;
			int texture_slice = -1;
			std::pair<int, int> key; // x, z
		};

		bool IsChunkVisible(const ChunkMesh& chunk, const Frustum& frustum) const;
		void EnsureTextureCapacity(int required_slices);
		void UploadHeightmapSlice(int slice, const std::vector<float>& heightmap);

		int chunk_size_;
		int heightmap_resolution_;
		int max_chunks_ = 512;
		GLuint heightmap_texture_ = 0;
		std::vector<int> free_slices_;
		int next_slice_ = 0;

		std::map<std::pair<int, int>, std::unique_ptr<ChunkMesh>> chunks_;
		std::vector<ChunkMesh*> visible_chunks_;

		mutable std::mutex mutex_;
		float last_world_scale_ = 1.0f;

	public:
		static std::shared_ptr<Shader> voxel_shader_;
	};

} // namespace Boidsish
