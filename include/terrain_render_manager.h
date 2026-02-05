#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "terrain.h"
#include "terrain_render_interface.h"

class Shader;

namespace Boidsish {

	struct Frustum;

	class TerrainRenderManager : public ITerrainRenderManager {
	public:
		TerrainRenderManager(int chunk_size = 32, int max_chunks = 512);
		~TerrainRenderManager();

		TerrainRenderManager(const TerrainRenderManager&) = delete;
		TerrainRenderManager& operator=(const TerrainRenderManager&) = delete;

		void RegisterChunk(
			std::pair<int, int>              chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset,
			const std::vector<OccluderQuad>& occluders = {}
		) override;

		void UnregisterChunk(std::pair<int, int> chunk_key) override;

		bool HasChunk(std::pair<int, int> chunk_key) const override;

		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos) override;

		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		) override;

		void RenderOccluders(Shader& shader) override;

		void CommitUpdates() {}

		void SetEvictionCallback(std::function<void(std::pair<int, int>)> callback) { eviction_callback_ = callback; }

		size_t GetRegisteredChunkCount() const override;
		size_t GetVisibleChunkCount() const override;

		int GetChunkSize() const override { return chunk_size_; }

		GLuint GetHeightmapTexture() const { return heightmap_texture_; }

		std::vector<glm::vec4> GetChunkInfo() const;

	private:
		struct ChunkInfo {
			int                       texture_slice;
			float                     min_y;
			float                     max_y;
			glm::vec2                 world_offset;
			std::vector<OccluderQuad> occluders;
		};

		struct alignas(16) InstanceData {
			glm::vec4 world_offset_and_slice;
			glm::vec4 bounds;
		};

		bool IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum) const;
		void CreateGridMesh();
		void EnsureTextureCapacity(int required_slices);
		void UploadHeightmapSlice(int slice, const std::vector<float>& heightmap, const std::vector<glm::vec3>& normals);

		int chunk_size_;
		int max_chunks_;
		int heightmap_resolution_;

		GLuint grid_vao_ = 0;
		GLuint grid_vbo_ = 0;
		GLuint grid_ebo_ = 0;
		GLuint instance_vbo_ = 0;
		GLuint heightmap_texture_ = 0;

		size_t grid_index_count_ = 0;

		std::map<std::pair<int, int>, ChunkInfo> chunks_;
		std::vector<int>                         free_slices_;
		int                                      next_slice_ = 0;

		std::vector<InstanceData> visible_instances_;
		size_t                    instance_buffer_capacity_ = 0;

		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};

		mutable std::mutex mutex_;

		std::function<void(std::pair<int, int>)> eviction_callback_;
	};

} // namespace Boidsish
