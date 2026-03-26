#include "mesh_terrain_render_manager.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include "biome_properties.h"
#include "constants.h"
#include "frustum.h"
#include "shader.h"

namespace Boidsish {

	MeshTerrainRenderManager::MeshTerrainRenderManager(int chunk_size) : chunk_size_(chunk_size) {
		mesh_shader_ = std::make_shared<Shader>("shaders/mesh_terrain.vert", "shaders/mesh_terrain.frag");

		// Create Biome UBO (binding 7)
		GLuint biome_ubo;
		glGenBuffers(1, &biome_ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, biome_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(BiomeShaderProperties) * kBiomes.size(), nullptr, GL_STATIC_DRAW);

		std::vector<BiomeShaderProperties> shader_biomes;
		for (const auto& b : kBiomes) {
			BiomeShaderProperties sb;
			sb.albedo_roughness = glm::vec4(b.albedo, b.roughness);
			sb.params = glm::vec4(b.metallic, b.detailStrength, b.detailScale, 0.0f);
			shader_biomes.push_back(sb);
		}
		glBufferSubData(
			GL_UNIFORM_BUFFER,
			0,
			sizeof(BiomeShaderProperties) * shader_biomes.size(),
			shader_biomes.data()
		);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		biome_ubo_ = biome_ubo;
	}

	MeshTerrainRenderManager::~MeshTerrainRenderManager() {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto& pair : chunks_) {
			_DestroyChunk(*pair.second);
		}
		if (biome_ubo_) {
			glDeleteBuffers(1, &biome_ubo_);
		}
	}

	void MeshTerrainRenderManager::RegisterChunk(
		std::pair<int, int>              chunk_key,
		const std::vector<glm::vec3>&    positions,
		const std::vector<glm::vec3>&    normals,
		const std::vector<glm::vec2>&    biomes,
		const std::vector<unsigned int>& indices,
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = chunks_.find(chunk_key);
		if (it != chunks_.end()) {
			_DestroyChunk(*it->second);
		} else {
			chunks_[chunk_key] = std::make_unique<ChunkMesh>();
		}

		auto& chunk = *chunks_[chunk_key];
		chunk.world_offset = world_offset;
		chunk.min_y = min_y;
		chunk.max_y = max_y;
		chunk.index_count = static_cast<GLsizei>(indices.size());

		// Interleave data: Pos(3), Normal(3), Biome(2) = 8 floats
		std::vector<float> vertex_data;
		vertex_data.reserve(positions.size() * 8);
		for (size_t i = 0; i < positions.size(); ++i) {
			vertex_data.push_back(positions[i].x);
			vertex_data.push_back(positions[i].y);
			vertex_data.push_back(positions[i].z);
			vertex_data.push_back(normals[i].x);
			vertex_data.push_back(normals[i].y);
			vertex_data.push_back(normals[i].z);
			vertex_data.push_back(biomes[i].x);
			vertex_data.push_back(biomes[i].y);
		}

		glGenVertexArrays(1, &chunk.vao);
		glGenBuffers(1, &chunk.vbo);
		glGenBuffers(1, &chunk.ebo);

		glBindVertexArray(chunk.vao);

		glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		// Position: loc 0
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal: loc 1
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Biome: loc 2
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	void MeshTerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = chunks_.find(chunk_key);
		if (it != chunks_.end()) {
			_DestroyChunk(*it->second);
			chunks_.erase(it);
		}
	}

	bool MeshTerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.find(chunk_key) != chunks_.end();
	}

	void MeshTerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale) {
		std::lock_guard<std::mutex> lock(mutex_);
		visible_chunks_.clear();

		float scaled_size = static_cast<float>(chunk_size_) * world_scale;

		for (auto& pair : chunks_) {
			auto& chunk = *pair.second;

			// Simple AABB frustum check
			glm::vec3 min_p = chunk.world_offset;
			glm::vec3 max_p = min_p + glm::vec3(scaled_size, 0, scaled_size);
			min_p.y = chunk.min_y;
			max_p.y = chunk.max_y;

			glm::vec3 center = (min_p + max_p) * 0.5f;
			glm::vec3 half_extents = (max_p - min_p) * 0.5f;

			bool inside = true;
			for (int i = 0; i < 6; ++i) {
				float r = half_extents.x * std::abs(frustum.planes[i].normal.x) +
						  half_extents.y * std::abs(frustum.planes[i].normal.y) +
						  half_extents.z * std::abs(frustum.planes[i].normal.z);
				float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
				if (d < -r) {
					inside = false;
					break;
				}
			}

			if (inside) {
				visible_chunks_.push_back(&chunk);
			}
		}

		// Sort by distance to camera
		std::sort(visible_chunks_.begin(), visible_chunks_.end(), [&](ChunkMesh* a, ChunkMesh* b) {
			float distA = glm::distance2(camera_pos, a->world_offset + glm::vec3(scaled_size*0.5f, (a->min_y+a->max_y)*0.5f, scaled_size*0.5f));
			float distB = glm::distance2(camera_pos, b->world_offset + glm::vec3(scaled_size*0.5f, (b->min_y+b->max_y)*0.5f, scaled_size*0.5f));
			return distA < distB;
		});
	}

	void MeshTerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const glm::vec2&                viewport_size,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		// Bind Biome UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);

		shader.setBool("uUseMeshMode", true);

		for (auto* chunk : visible_chunks_) {
			shader.setMat4("model", glm::translate(glm::mat4(1.0f), chunk->world_offset));
			glBindVertexArray(chunk->vao);
			glDrawElements(GL_TRIANGLES, chunk->index_count, GL_UNSIGNED_INT, 0);
		}

		glBindVertexArray(0);
		shader.setBool("uUseMeshMode", false);
	}

	size_t MeshTerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t MeshTerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return visible_chunks_.size();
	}

	void MeshTerrainRenderManager::BindTerrainData(class ShaderBase& shader_base) const {
		shader_base.use();
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
	}

	void MeshTerrainRenderManager::_DestroyChunk(ChunkMesh& chunk) {
		if (chunk.vao) glDeleteVertexArrays(1, &chunk.vao);
		if (chunk.vbo) glDeleteBuffers(1, &chunk.vbo);
		if (chunk.ebo) glDeleteBuffers(1, &chunk.ebo);
		chunk.vao = chunk.vbo = chunk.ebo = 0;
	}

} // namespace Boidsish
