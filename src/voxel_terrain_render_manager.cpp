#include "voxel_terrain_render_manager.h"

#include <algorithm>
#include <iostream>

#include "graphics.h"
#include <shader.h>

namespace Boidsish {

	std::shared_ptr<Shader> VoxelTerrainRenderManager::voxel_shader_ = nullptr;

	VoxelTerrainRenderManager::VoxelTerrainRenderManager(int chunk_size):
		chunk_size_(chunk_size), heightmap_resolution_(chunk_size + 1) {
		EnsureTextureCapacity(max_chunks_);
	}

	VoxelTerrainRenderManager::~VoxelTerrainRenderManager() {
		for (auto& pair : chunks_) {
			if (pair.second->vao)
				glDeleteVertexArrays(1, &pair.second->vao);
			if (pair.second->vbo)
				glDeleteBuffers(1, &pair.second->vbo);
			if (pair.second->ebo)
				glDeleteBuffers(1, &pair.second->ebo);
		}
		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
	}

	void VoxelTerrainRenderManager::RegisterChunk(std::pair<int, int> chunk_key, const TerrainGenerationResult& result) {
		std::lock_guard<std::mutex> lock(mutex_);

		const int          res = heightmap_resolution_;
		std::vector<float> heightmap(res * res);

		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z; // X-major
				int dst_idx = z * res + x; // Z-major

				heightmap[dst_idx] = result.positions[src_idx].y;
			}
		}

		// Remove existing chunk if it exists
		int existing_slice = -1;
		auto it = chunks_.find(chunk_key);
		if (it != chunks_.end()) {
			existing_slice = it->second->texture_slice;
			if (it->second->vao)
				glDeleteVertexArrays(1, &it->second->vao);
			if (it->second->vbo)
				glDeleteBuffers(1, &it->second->vbo);
			if (it->second->ebo)
				glDeleteBuffers(1, &it->second->ebo);
			chunks_.erase(it);
		}

		auto chunk = std::make_unique<ChunkMesh>();
		chunk->key = chunk_key;
		chunk->world_offset = result.world_offset;

		chunk->min_corner = glm::vec3(0, result.proxy.minY, 0);
		chunk->max_corner = glm::vec3(chunk_size_, result.proxy.maxY, chunk_size_);

		// Assign or reuse slice
		if (existing_slice != -1) {
			chunk->texture_slice = existing_slice;
		} else if (!free_slices_.empty()) {
			chunk->texture_slice = free_slices_.back();
			free_slices_.pop_back();
		} else {
			if (next_slice_ >= max_chunks_) {
				EnsureTextureCapacity(max_chunks_ + 128);
			}
			chunk->texture_slice = next_slice_++;
		}

		UploadHeightmapSlice(chunk->texture_slice, heightmap);

		std::vector<float>        vertex_data;
		std::vector<unsigned int> index_data;

		auto add_quad = [&](const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4, const glm::vec3& n) {
			unsigned int base_idx = vertex_data.size() / 8;

			// Vertex format: pos(3), normal(3), tex(2)
			auto add_vert = [&](const glm::vec3& p) {
				vertex_data.push_back(p.x);
				vertex_data.push_back(p.y);
				vertex_data.push_back(p.z);
				vertex_data.push_back(n.x);
				vertex_data.push_back(n.y);
				vertex_data.push_back(n.z);
				vertex_data.push_back(0.0f); // Tex U
				vertex_data.push_back(0.0f); // Tex V
			};

			add_vert(p1);
			add_vert(p2);
			add_vert(p3);
			add_vert(p4);

			index_data.push_back(base_idx + 0);
			index_data.push_back(base_idx + 1);
			index_data.push_back(base_idx + 2);
			index_data.push_back(base_idx + 0);
			index_data.push_back(base_idx + 2);
			index_data.push_back(base_idx + 3);
		};

		auto get_height = [&](int i, int j) {
			i = std::clamp(i, 0, chunk_size_);
			j = std::clamp(j, 0, chunk_size_);
			return result.positions[i * res + j].y;
		};

		float step = 1.0f; // Assuming positions are at integer intervals in local space

		for (int i = 0; i < chunk_size_; ++i) {
			for (int j = 0; j < chunk_size_; ++j) {
				float h = get_height(i, j);

				// Top face
				glm::vec3 p1(i * step, h, j * step);
				glm::vec3 p2((i + 1) * step, h, j * step);
				glm::vec3 p3((i + 1) * step, h, (j + 1) * step);
				glm::vec3 p4(i * step, h, (j + 1) * step);
				add_quad(p1, p2, p3, p4, glm::vec3(0, 1, 0));

				// Side faces (only if higher than neighbor)
				// North (+Z)
				float hn = (j < chunk_size_ - 1) ? get_height(i, j + 1) : -1000.0f;
				if (h > hn) {
					add_quad(p4, p3, glm::vec3((i + 1) * step, hn, (j + 1) * step), glm::vec3(i * step, hn, (j + 1) * step), glm::vec3(0, 0, 1));
				}
				// South (-Z)
				float hs = (j > 0) ? get_height(i, j - 1) : -1000.0f;
				if (h > hs) {
					add_quad(p2, p1, glm::vec3(i * step, hs, j * step), glm::vec3((i + 1) * step, hs, j * step), glm::vec3(0, 0, -1));
				}
				// East (+X)
				float he = (i < chunk_size_ - 1) ? get_height(i + 1, j) : -1000.0f;
				if (h > he) {
					add_quad(p3, p2, glm::vec3((i + 1) * step, he, j * step), glm::vec3((i + 1) * step, he, (j + 1) * step), glm::vec3(1, 0, 0));
				}
				// West (-X)
				float hw = (i > 0) ? get_height(i - 1, j) : -1000.0f;
				if (h > hw) {
					add_quad(p1, p4, glm::vec3(i * step, hw, (j + 1) * step), glm::vec3(i * step, hw, j * step), glm::vec3(-1, 0, 0));
				}
			}
		}

		chunk->index_count = index_data.size();

		glGenVertexArrays(1, &chunk->vao);
		glGenBuffers(1, &chunk->vbo);
		glGenBuffers(1, &chunk->ebo);

		glBindVertexArray(chunk->vao);
		glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
		glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_data.size() * sizeof(unsigned int), index_data.data(), GL_STATIC_DRAW);

		// Position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Texcoord
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		chunks_[chunk_key] = std::move(chunk);
	}

	void VoxelTerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		auto it = chunks_.find(chunk_key);
		if (it != chunks_.end()) {
			if (it->second->vao)
				glDeleteVertexArrays(1, &it->second->vao);
			if (it->second->vbo)
				glDeleteBuffers(1, &it->second->vbo);
			if (it->second->ebo)
				glDeleteBuffers(1, &it->second->ebo);

			if (it->second->texture_slice != -1) {
				free_slices_.push_back(it->second->texture_slice);
			}

			chunks_.erase(it);
		}
	}

	bool VoxelTerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		return chunks_.count(chunk_key) > 0;
	}

	bool VoxelTerrainRenderManager::IsChunkVisible(const ChunkMesh& chunk, const Frustum& frustum) const {
		glm::vec3 world_min = chunk.min_corner * last_world_scale_ + chunk.world_offset;
		glm::vec3 world_max = chunk.max_corner * last_world_scale_ + chunk.world_offset;
		return frustum.IsBoxInFrustum(world_min, world_max);
	}

	void VoxelTerrainRenderManager::EnsureTextureCapacity(int required_slices) {
		if (heightmap_texture_ && required_slices <= max_chunks_) {
			return;
		}

		int new_capacity = std::max(max_chunks_, required_slices);

		if (heightmap_texture_) {
			glDeleteTextures(1, &heightmap_texture_);
			heightmap_texture_ = 0;
			next_slice_ = 0;
			free_slices_.clear();
			// Chunks lost their slices, but they will be re-uploaded on RegisterChunk
			for (auto& pair : chunks_) {
				pair.second->texture_slice = -1;
			}
		}

		max_chunks_ = new_capacity;

		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);

		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_R32F, // We only need height for decor placement
			heightmap_resolution_,
			heightmap_resolution_,
			max_chunks_,
			0,
			GL_RED,
			GL_FLOAT,
			nullptr
		);

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void VoxelTerrainRenderManager::UploadHeightmapSlice(int slice, const std::vector<float>& heightmap) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			0,
			0,
			slice,
			heightmap_resolution_,
			heightmap_resolution_,
			1,
			GL_RED,
			GL_FLOAT,
			heightmap.data()
		);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void VoxelTerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& /*camera_pos*/, float world_scale) {
		std::lock_guard<std::mutex> lock(mutex_);
		last_world_scale_ = world_scale;
		visible_chunks_.clear();
		for (auto& pair : chunks_) {
			if (IsChunkVisible(*pair.second, frustum)) {
				visible_chunks_.push_back(pair.second.get());
			}
		}
	}

	void VoxelTerrainRenderManager::Render(
		Shader&                         /*original_shader*/,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const glm::vec2&                viewport_size,
		const std::optional<glm::vec4>& clip_plane,
		float                           /*tess_quality_multiplier*/,
		bool                            is_shadow_pass
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!voxel_shader_)
			return;

		voxel_shader_->use();
		voxel_shader_->setMat4("view", view);
		voxel_shader_->setMat4("projection", projection);
		voxel_shader_->setBool("uIsShadowPass", is_shadow_pass);
		voxel_shader_->setVec2("uViewportSize", viewport_size);

		if (clip_plane) {
			voxel_shader_->setVec4("clipPlane", *clip_plane);
		} else {
			voxel_shader_->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		// Ensure shadow maps are bound to unit 4 if not in shadow pass
		if (!is_shadow_pass) {
			voxel_shader_->setInt("shadowMaps", 4);
		}

		for (auto* chunk : visible_chunks_) {
			glm::mat4 model = glm::translate(glm::mat4(1.0f), chunk->world_offset);
			model = glm::scale(model, glm::vec3(last_world_scale_));
			voxel_shader_->setMat4("model", model);

			glBindVertexArray(chunk->vao);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(chunk->index_count), GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
	}

	size_t VoxelTerrainRenderManager::GetRegisteredChunkCount() const {
		return chunks_.size();
	}

	size_t VoxelTerrainRenderManager::GetVisibleChunkCount() const {
		return visible_chunks_.size();
	}

	std::vector<glm::vec4> VoxelTerrainRenderManager::GetChunkInfo() const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<glm::vec4>      result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				glm::vec4(
					chunk->world_offset.x,
					chunk->world_offset.z,
					static_cast<float>(chunk->texture_slice),
					static_cast<float>(chunk_size_)
				)
			);
		}
		return result;
	}

} // namespace Boidsish
