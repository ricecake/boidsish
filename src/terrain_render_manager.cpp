#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>

#include "graphics.h"
#include <shader.h>

namespace Boidsish {

	TerrainRenderManager::TerrainRenderManager(int chunk_size, int max_chunks):
		chunk_size_(chunk_size), max_chunks_(max_chunks), heightmap_resolution_(chunk_size + 1) {
		glGenBuffers(1, &instance_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
		instance_buffer_capacity_ = max_chunks * sizeof(InstanceData);
		glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

		CreateGridMesh();
		EnsureTextureCapacity(max_chunks);
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (grid_vao_)
			glDeleteVertexArrays(1, &grid_vao_);
		if (grid_vbo_)
			glDeleteBuffers(1, &grid_vbo_);
		if (grid_ebo_)
			glDeleteBuffers(1, &grid_ebo_);
		if (instance_vbo_)
			glDeleteBuffers(1, &instance_vbo_);
		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
	}

	void TerrainRenderManager::CreateGridMesh() {
		const int num_verts = heightmap_resolution_ * heightmap_resolution_;
		const int num_quads = chunk_size_ * chunk_size_;

		std::vector<float> vertices;
		vertices.reserve(num_verts * 5);

		for (int z = 0; z < heightmap_resolution_; ++z) {
			for (int x = 0; x < heightmap_resolution_; ++x) {
				vertices.push_back(static_cast<float>(x));
				vertices.push_back(0.0f);
				vertices.push_back(static_cast<float>(z));
				vertices.push_back(static_cast<float>(x) / chunk_size_);
				vertices.push_back(static_cast<float>(z) / chunk_size_);
			}
		}

		std::vector<unsigned int> indices;
		indices.reserve(num_quads * 4);

		for (int z = 0; z < chunk_size_; ++z) {
			for (int x = 0; x < chunk_size_; ++x) {
				int i0 = z * heightmap_resolution_ + x;
				int i1 = z * heightmap_resolution_ + (x + 1);
				int i2 = (z + 1) * heightmap_resolution_ + (x + 1);
				int i3 = (z + 1) * heightmap_resolution_ + x;
				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
				indices.push_back(i3);
			}
		}

		grid_index_count_ = indices.size();

		glGenVertexArrays(1, &grid_vao_);
		glBindVertexArray(grid_vao_);

		glGenBuffers(1, &grid_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glGenBuffers(1, &grid_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

		glVertexAttribPointer(
			3,
			4,
			GL_FLOAT,
			GL_FALSE,
			sizeof(InstanceData),
			(void*)offsetof(InstanceData, world_offset_and_slice)
		);
		glEnableVertexAttribArray(3);
		glVertexAttribDivisor(3, 1);

		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, bounds));
		glEnableVertexAttribArray(4);
		glVertexAttribDivisor(4, 1);

		glBindVertexArray(0);
	}

	void TerrainRenderManager::EnsureTextureCapacity(int required_slices) {
		if (heightmap_texture_ && required_slices <= max_chunks_) {
			return;
		}

		GLint max_layers = 0;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
		if (max_layers <= 0)
			max_layers = 512;

		int new_capacity = std::max(max_chunks_, required_slices);
		if (new_capacity > max_layers) {
			new_capacity = max_layers;
		}

		if (heightmap_texture_ && new_capacity <= max_chunks_) {
			return;
		}

		if (heightmap_texture_) {
			glDeleteTextures(1, &heightmap_texture_);
			heightmap_texture_ = 0;
			next_slice_ = 0;
			free_slices_.clear();
			chunks_.clear();
		}

		max_chunks_ = new_capacity;

		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);

		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_RGBA16F,
			heightmap_resolution_,
			heightmap_resolution_,
			max_chunks_,
			0,
			GL_RGBA,
			GL_FLOAT,
			nullptr
		);

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int                           slice,
		const std::vector<float>&     heightmap,
		const std::vector<glm::vec3>& normals
	) {
		const int num_pixels = heightmap_resolution_ * heightmap_resolution_;

		std::vector<float> packed_data;
		packed_data.reserve(num_pixels * 4);

		for (int i = 0; i < num_pixels; ++i) {
			packed_data.push_back(heightmap[i]);
			packed_data.push_back(normals[i].x);
			packed_data.push_back(normals[i].y);
			packed_data.push_back(normals[i].z);
		}

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
			GL_RGBA,
			GL_FLOAT,
			packed_data.data()
		);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::RegisterChunk(
		std::pair<int, int>              chunk_key,
		const std::vector<glm::vec3>&    positions,
		const std::vector<glm::vec3>&    normals,
		const std::vector<unsigned int>& indices,
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset,
		const std::vector<OccluderQuad>& occluders
	) {
		bool                should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;

		const int              res = heightmap_resolution_;
		std::vector<float>     heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);

		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z;
				int dst_idx = z * res + x;

				heightmap[dst_idx] = positions[src_idx].y;
				reordered_normals[dst_idx] = normals[src_idx];
			}
		}

		{
			std::lock_guard<std::mutex> lock(mutex_);

			auto it = chunks_.find(chunk_key);
			if (it != chunks_.end()) {
				UploadHeightmapSlice(it->second.texture_slice, heightmap, reordered_normals);
				it->second.min_y = min_y;
				it->second.max_y = max_y;
				it->second.occluders = occluders;
				return;
			}

			int slice;
			GLuint query;
			glGenQueries(1, &query);

			if (!free_slices_.empty()) {
				slice = free_slices_.back();
				free_slices_.pop_back();
			} else {
				if (next_slice_ >= max_chunks_) {
					GLint max_layers = 0;
					glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);

					if (max_chunks_ >= max_layers) {
						glm::vec2 camera_pos_2d(last_camera_pos_.x, last_camera_pos_.z);

						float               max_dist_sq = -1.0f;
						std::pair<int, int> farthest_key;

						for (const auto& [key, chunk] : chunks_) {
							glm::vec2 chunk_center(
								chunk.world_offset.x + chunk_size_ * 0.5f,
								chunk.world_offset.y + chunk_size_ * 0.5f
							);
							float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);
							if (dist_sq > max_dist_sq) {
								max_dist_sq = dist_sq;
								farthest_key = key;
							}
						}

						if (max_dist_sq >= 0) {
							auto evict_it = chunks_.find(farthest_key);
							if (evict_it != chunks_.end()) {
								slice = evict_it->second.texture_slice;
								chunks_.erase(evict_it);
								evicted_chunk_key = farthest_key;
								should_notify_eviction = true;
							} else {
								return;
							}
						} else {
							return;
						}
					} else {
						int new_capacity = std::min(max_chunks_ * 2, max_layers);
						EnsureTextureCapacity(new_capacity);
						slice = next_slice_++;
					}
				} else {
					slice = next_slice_++;
				}
			}

			UploadHeightmapSlice(slice, heightmap, reordered_normals);

			ChunkInfo info{};
			info.texture_slice = slice;
			info.min_y = min_y;
			info.max_y = max_y;
			info.world_offset = glm::vec2(world_offset.x, world_offset.z);
			info.occluders = occluders;
			info.occlusion_query = query;
			info.is_occluded = false;

			chunks_[chunk_key] = info;
		}

		if (should_notify_eviction && eviction_callback_) {
			eviction_callback_(evicted_chunk_key);
		}
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = chunks_.find(chunk_key);
		if (it == chunks_.end()) {
			return;
		}

		free_slices_.push_back(it->second.texture_slice);
		if (it->second.occlusion_query != 0) {
			glDeleteQueries(1, &it->second.occlusion_query);
		}
		chunks_.erase(it);
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.count(chunk_key) > 0;
	}

	bool TerrainRenderManager::IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum) const {
		glm::vec3 min_corner(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y);
		glm::vec3 max_corner(chunk.world_offset.x + chunk_size_, chunk.max_y, chunk.world_offset.y + chunk_size_);

		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f;

		for (int i = 0; i < 6; ++i) {
			float r = half_size.x * std::abs(frustum.planes[i].normal.x) +
				half_size.y * std::abs(frustum.planes[i].normal.y) + half_size.z * std::abs(frustum.planes[i].normal.z);

			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;

			if (d < -r) {
				return false;
			}
		}

		return true;
	}

	void TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos) {
		std::lock_guard<std::mutex> lock(mutex_);

		last_camera_pos_ = camera_pos;

		visible_instances_.clear();
		visible_instances_.reserve(chunks_.size());

		struct VisibleChunk {
			InstanceData instance;
			float        distance_sq;
		};

		std::vector<VisibleChunk> visible_chunks;
		visible_chunks.reserve(chunks_.size());

		glm::vec2 camera_pos_2d(camera_pos.x, camera_pos.z);

		for (const auto& [key, chunk] : chunks_) {
			if (IsChunkVisible(chunk, frustum)) {
				if (chunk.is_occluded) {
					continue;
				}

				InstanceData instance{};
				instance.world_offset_and_slice = glm::vec4(
					chunk.world_offset.x,
					0.0f,
					chunk.world_offset.y,
					static_cast<float>(chunk.texture_slice)
				);
				instance.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);

				glm::vec2 chunk_center(
					chunk.world_offset.x + chunk_size_ * 0.5f,
					chunk.world_offset.y + chunk_size_ * 0.5f
				);
				float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);

				visible_chunks.push_back({instance, dist_sq});
			}
		}

		std::sort(visible_chunks.begin(), visible_chunks.end(), [](const VisibleChunk& a, const VisibleChunk& b) {
			return a.distance_sq < b.distance_sq;
		});

		for (const auto& vc : visible_chunks) {
			visible_instances_.push_back(vc.instance);
		}

		if (!visible_instances_.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

			size_t required_size = visible_instances_.size() * sizeof(InstanceData);
			if (required_size > instance_buffer_capacity_) {
				instance_buffer_capacity_ = required_size * 2;
				glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

				glBindVertexArray(grid_vao_);
				glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

				glVertexAttribPointer(
					3,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(InstanceData),
					(void*)offsetof(InstanceData, world_offset_and_slice)
				);
				glEnableVertexAttribArray(3);
				glVertexAttribDivisor(3, 1);

				glVertexAttribPointer(
					4,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(InstanceData),
					(void*)offsetof(InstanceData, bounds)
				);
				glEnableVertexAttribArray(4);
				glVertexAttribDivisor(4, 1);

				glBindVertexArray(0);
			}

			glBufferSubData(GL_ARRAY_BUFFER, 0, required_size, visible_instances_.data());
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	void TerrainRenderManager::RenderOccluders(Shader& shader) {
		std::lock_guard<std::mutex> lock(mutex_);

		static GLuint vao = 0, vbo = 0;
		if (vao == 0) {
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);
		}

		// Gather all occluder vertices in world space for a single batch
		std::vector<glm::vec3> all_vertices;
		for (const auto& [key, chunk] : chunks_) {
			glm::vec3 world_origin(chunk.world_offset.x, 0, chunk.world_offset.y);
			for (const auto& quad : chunk.occluders) {
				// We render as triangles for solid occlusion
				all_vertices.push_back(quad.corners[0] + world_origin);
				all_vertices.push_back(quad.corners[1] + world_origin);
				all_vertices.push_back(quad.corners[2] + world_origin);

				all_vertices.push_back(quad.corners[0] + world_origin);
				all_vertices.push_back(quad.corners[2] + world_origin);
				all_vertices.push_back(quad.corners[3] + world_origin);
			}
		}

		if (all_vertices.empty())
			return;

		shader.use();
		shader.setMat4("model", glm::mat4(1.0f));
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, all_vertices.size() * sizeof(glm::vec3), all_vertices.data(), GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(all_vertices.size()));
		glBindVertexArray(0);
	}

	void TerrainRenderManager::PerformOcclusionQueries(
		const glm::mat4& view,
		const glm::mat4& projection,
		const Frustum&   frustum,
		Shader&          shader
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		// 1. Update is_occluded from previous frame results
		for (auto& [key, chunk] : chunks_) {
			if (!chunk.query_issued)
				continue;

			GLuint available = 0;
			glGetQueryObjectuiv(chunk.occlusion_query, GL_QUERY_RESULT_AVAILABLE, &available);
			if (available) {
				GLuint samples = 0;
				glGetQueryObjectuiv(chunk.occlusion_query, GL_QUERY_RESULT, &samples);
				chunk.is_occluded = (samples == 0);
				chunk.query_issued = false;
			}
		}

		// 2. Issue new queries for chunks in the frustum
		// We use a shared VAO/VBO for bounding boxes
		static GLuint bbox_vao = 0, bbox_vbo = 0, bbox_ebo = 0;
		if (bbox_vao == 0) {
			glGenVertexArrays(1, &bbox_vao);
			glGenBuffers(1, &bbox_vbo);
			glGenBuffers(1, &bbox_ebo);

			float vertices[] = {
				0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
			};

			unsigned int indices[] = {
				0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 1, 5, 5, 4, 0,
				2, 3, 7, 7, 6, 2, 1, 2, 6, 6, 5, 1, 0, 3, 7, 7, 4, 0,
			};

			glBindVertexArray(bbox_vao);
			glBindBuffer(GL_ARRAY_BUFFER, bbox_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbox_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}

		glBindVertexArray(bbox_vao);
		GLint modelLoc = glGetUniformLocation(shader.ID, "model");

		for (auto& [key, chunk] : chunks_) {
			if (IsChunkVisible(chunk, frustum)) {
				// Set model matrix for the bounding box
				glm::mat4 model = glm::translate(
					glm::mat4(1.0f),
					glm::vec3(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y)
				);
				model = glm::scale(model, glm::vec3(chunk_size_, chunk.max_y - chunk.min_y, chunk_size_));
				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

				glBeginQuery(GL_ANY_SAMPLES_PASSED, chunk.occlusion_query);
				glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
				glEndQuery(GL_ANY_SAMPLES_PASSED);
				chunk.query_issued = true;
			} else {
				// Prevent sticky occlusion when re-entering frustum
				chunk.is_occluded = false;
			}
		}
		glBindVertexArray(0);
	}

	void TerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (visible_instances_.empty() || grid_vao_ == 0 || grid_index_count_ == 0) {
			return;
		}

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uTessLevelMax", 64.0f);
		shader.setFloat("uTessLevelMin", 1.0f);
		shader.setInt("uChunkSize", chunk_size_);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.setInt("uHeightmap", 0);

		glBindVertexArray(grid_vao_);

		glPatchParameteri(GL_PATCH_VERTICES, 4);

		glDrawElementsInstanced(
			GL_PATCHES,
			static_cast<GLsizei>(grid_index_count_),
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(visible_instances_.size())
		);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return visible_instances_.size();
	}

	std::vector<glm::vec4> TerrainRenderManager::GetChunkInfo() const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<glm::vec4>      result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				glm::vec4(
					chunk.world_offset.x,
					chunk.world_offset.y,
					static_cast<float>(chunk.texture_slice),
					static_cast<float>(chunk_size_)
				)
			);
		}
		return result;
	}

} // namespace Boidsish
