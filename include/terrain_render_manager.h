#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader;
class ShaderBase;
class ComputeShader;

namespace Boidsish {

	struct Frustum;

	/**
	 * @brief High-performance instanced terrain rendering with heightmap lookup.
	 *
	 * Architecture:
	 * - Single flat grid mesh (1x1 quad) instanced for all visible patches
	 * - Each chunk is divided into 8x8 patches (64 total) for fine-grained culling
	 * - Heightmap stored in texture array (one slice per chunk)
	 * - GPU-side culling (frustum + Hi-Z) populates an indirect draw buffer
	 * - Tessellation shader samples heightmap for vertex displacement
	 */
	class TerrainRenderManager {
	public:
		TerrainRenderManager(int chunk_size = 32, int max_chunks = 512);
		~TerrainRenderManager();

		// Non-copyable
		TerrainRenderManager(const TerrainRenderManager&) = delete;
		TerrainRenderManager& operator=(const TerrainRenderManager&) = delete;

		void RegisterChunk(
			std::pair<int, int>              chunk_key,
			const std::vector<glm::vec3>&    positions,
			const std::vector<glm::vec3>&    normals,
			const std::vector<glm::vec2>&    biomes,
			const std::vector<unsigned int>& indices,
			float                            min_y,
			float                            max_y,
			const glm::vec3&                 world_offset
		);

		void UnregisterChunk(std::pair<int, int> chunk_key);
		bool HasChunk(std::pair<int, int> chunk_key) const;
		void PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale = 1.0f);
		void Render(
			Shader&                         shader,
			const glm::mat4&                view,
			const glm::mat4&                projection,
			const glm::vec2&                viewport_size,
			const std::optional<glm::vec4>& clip_plane,
			float                           tess_quality_multiplier
		);

		void CommitUpdates() {}
		void SetEvictionCallback(std::function<void(std::pair<int, int>)> callback) { eviction_callback_ = callback; }

		size_t GetRegisteredChunkCount() const;
		size_t GetVisibleChunkCount() const;
		int GetChunkSize() const { return chunk_size_; }
		GLuint GetHeightmapTexture() const { return heightmap_texture_; }
		GLuint GetBiomeTexture() const { return biome_texture_; }

		std::vector<glm::vec4> GetChunkInfo(float world_scale) const;

		struct DecorChunkData {
			std::pair<int, int> key;
			glm::vec2           world_offset;
			float               slice;
			float               chunk_size;
			uint32_t            update_count;
		};
		std::vector<DecorChunkData> GetDecorChunkData(float world_scale) const;

		void BindTerrainData(ShaderBase& shader_base) const;

		void SetHiZData(GLuint texture, int width, int height, int mips, const glm::mat4& prev_vp) {
			hiz_texture_ = texture;
			hiz_width_ = width;
			hiz_height_ = height;
			hiz_mip_count_ = mips;
			hiz_prev_vp_ = prev_vp;
		}

		void SetNoise(const GLuint& noise, const GLuint& curl, const GLuint& extra = 0) {
			if (noise != 0)
				noise_texture_ = noise;
			if (curl != 0)
				curl_texture_ = curl;
			if (extra != 0)
				extra_noise_texture_ = extra;
		}

	private:
		void UpdateGridTextures(float world_scale);
		void GenerateMaxHeightMips();
		void UpdateChunkMetadata();

		struct ChunkInfo {
			int       texture_slice;
			float     min_y;
			float     max_y;
			glm::vec2 world_offset;
			uint32_t  update_count = 0;
			int       gpu_index = -1;
		};

		struct ChunkMetadataGPU {
			glm::vec4 world_offset_slice; // x, z, slice, active
			glm::vec4 bounds;             // min_y, max_y, 0, 0
		};

		bool IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum, float world_scale) const;
		void CreateGridMesh();
		void EnsureTextureCapacity(int required_slices);
		void UploadHeightmapSlice(int slice, const std::vector<float>& h, const std::vector<glm::vec3>& n, const std::vector<glm::vec2>& b);

		int chunk_size_;
		int max_chunks_;
		int heightmap_resolution_;

		GLuint grid_vao_ = 0, grid_vbo_ = 0, grid_ebo_ = 0;
		GLuint heightmap_texture_ = 0, biome_texture_ = 0;
		GLuint noise_texture_ = 0, curl_texture_ = 0, extra_noise_texture_ = 0, biome_ubo_ = 0;

		GLuint chunk_metadata_ssbo_ = 0, visible_patches_ssbo_ = 0, indirect_buffer_ = 0;
		std::unique_ptr<ComputeShader> cull_shader_;

		// Cached uniform locations for cull_shader_
		GLint cull_num_chunks_loc_ = -1;
		GLint cull_max_visible_patches_loc_ = -1;
		GLint cull_chunk_size_loc_ = -1;
		GLint cull_frustum_planes_loc_[6];
		GLint cull_camera_pos_loc_ = -1;
		GLint cull_chunk_grid_loc_ = -1;
		GLint cull_max_height_grid_loc_ = -1;
		GLint cull_heightmap_array_loc_ = -1;

		GLuint chunk_grid_texture_ = 0, max_height_grid_texture_ = 0, terrain_data_ubo_ = 0;
		std::unique_ptr<ComputeShader> grid_mip_shader_;

		size_t grid_index_count_ = 0;
		std::map<std::pair<int, int>, ChunkInfo> chunks_;
		std::vector<int> free_slices_;
		int next_slice_ = 0;

		std::vector<int> free_gpu_indices_;
		int next_gpu_index_ = 0;
		bool chunk_metadata_dirty_ = false;

		GLuint hiz_texture_ = 0;
		int hiz_width_ = 0, hiz_height_ = 0, hiz_mip_count_ = 0;
		glm::mat4 hiz_prev_vp_{1.0f};

		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};
		float     last_world_scale_ = 1.0f;

		int       last_grid_origin_x_ = 0;
		int       last_grid_origin_z_ = 0;
		float     last_grid_world_scale_ = -1.0f;

		size_t    cached_visible_patch_count_ = 0;

		mutable std::recursive_mutex mutex_;
		std::function<void(std::pair<int, int>)> eviction_callback_;
	};

}
