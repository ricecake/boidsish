#include "terrain_generator.h"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	bool isChunkInFrustum(const Frustum& frustum, int chunkX, int chunkZ, int chunkSize, float amplitude) {
		glm::vec3 center(
			chunkX * chunkSize + chunkSize / 2.0f,
			amplitude / 2.0f,
			chunkZ * chunkSize + chunkSize / 2.0f
		);
		glm::vec3 halfSize(chunkSize / 2.0f, amplitude / 2.0f, chunkSize / 2.0f);

		for (int i = 0; i < 6; ++i) {
			float r = halfSize.x * std::abs(frustum.planes[i].normal.x) +
			          halfSize.y * std::abs(frustum.planes[i].normal.y) +
			          halfSize.z * std::abs(frustum.planes[i].normal.z);
			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
			if (d < -r) {
				return false;
			}
		}
		return true;
	}

	TerrainGenerator::TerrainGenerator(int seed): perlin_noise_(seed) {}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		float height_factor = std::max(1.0f, camera.y / 10.0f);
		int   dynamic_view_distance = std::min(16, static_cast<int>(view_distance_ * height_factor));

		// Load chunks based on frustum and dynamic view distance
		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				if (isChunkInFrustum(frustum, x, z, chunk_size_, amplitude_)) {
					std::pair<int, int> chunk_coord = {x, z};
					if (chunk_cache_.find(chunk_coord) == chunk_cache_.end()) {
						chunk_cache_[chunk_coord] = generateChunk(x, z);
					}
				}
			}
		}

		// Unload chunks
		std::vector<std::pair<int, int>> to_remove;
		for (auto const& [key, val] : chunk_cache_) {
			int dx = key.first - current_chunk_x;
			int dz = key.second - current_chunk_z;
			if (std::abs(dx) > dynamic_view_distance || std::abs(dz) > dynamic_view_distance) {
				to_remove.push_back(key);
			}
		}

		for (const auto& key : to_remove) {
			chunk_cache_.erase(key);
		}
	}

	std::vector<std::shared_ptr<Terrain>> TerrainGenerator::getVisibleChunks() {
		std::vector<std::shared_ptr<Terrain>> visible_chunks;
		for (auto const& [key, val] : chunk_cache_) {
			if (val) {
				visible_chunks.push_back(val);
			}
		}
		return visible_chunks;
	}

	std::shared_ptr<Terrain> TerrainGenerator::generateChunk(int chunkX, int chunkZ) {
		const int num_vertices_x = chunk_size_ + 1;
		const int num_vertices_z = chunk_size_ + 1;

		std::vector<std::vector<float>> heightmap(num_vertices_x, std::vector<float>(num_vertices_z));
		std::vector<float>              vertexData;
		std::vector<unsigned int>       indices;
		bool                            has_terrain = false;

		// Generate heightmap
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float worldX = (chunkX * chunk_size_ + i);
				float worldZ = (chunkZ * chunk_size_ + j);
				float noise = perlin_noise_.octave2D_01(worldX * scale_, worldZ * scale_, 4);

				if (noise > threshold_) {
					heightmap[i][j] = (noise - threshold_) * amplitude_;
					has_terrain = true;
				} else {
					heightmap[i][j] = 0.0f; // Flush with floor
				}
			}
		}

		// If chunk is completely flat, don't generate a mesh for it
		if (!has_terrain) {
			return nullptr;
		}

		// Generate vertices and normals
		vertexData.reserve(num_vertices_x * num_vertices_z * 6);
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float y = heightmap[i][j];

				// Vertex position
				vertexData.push_back(i);
				vertexData.push_back(y);
				vertexData.push_back(j);

				// Calculate normal
				float hL = (i > 0) ? heightmap[i - 1][j] : y;
				float hR = (i < chunk_size_) ? heightmap[i + 1][j] : y;
				float hD = (j > 0) ? heightmap[i][j - 1] : y;
				float hU = (j < chunk_size_) ? heightmap[i][j + 1] : y;

				glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f, hD - hU));
				vertexData.push_back(normal.x);
				vertexData.push_back(normal.y);
				vertexData.push_back(normal.z);
			}
		}

		// Generate indices for a full grid with CCW winding
		indices.reserve(chunk_size_ * chunk_size_ * 6);
		for (int i = 0; i < chunk_size_; ++i) {
			for (int j = 0; j < chunk_size_; ++j) {
				// First triangle (CCW)
				indices.push_back(i * num_vertices_z + j);
				indices.push_back(i * num_vertices_z + j + 1);
				indices.push_back((i + 1) * num_vertices_z + j);

				// Second triangle (CCW)
				indices.push_back((i + 1) * num_vertices_z + j);
				indices.push_back(i * num_vertices_z + j + 1);
				indices.push_back((i + 1) * num_vertices_z + j + 1);
			}
		}

		auto terrain_chunk = std::make_shared<Terrain>(vertexData, indices);
		terrain_chunk->SetPosition(chunkX * chunk_size_, 0, chunkZ * chunk_size_);
		return terrain_chunk;
	}

} // namespace Boidsish
