#include "terrain_generator.h"

#include <vector>

#include <glm/glm.hpp>

namespace Boidsish {

	TerrainGenerator::TerrainGenerator(int seed): perlin_noise_(seed) {}

	void TerrainGenerator::update(const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		// Load chunks
		for (int x = current_chunk_x - view_distance_; x <= current_chunk_x + view_distance_; ++x) {
			for (int z = current_chunk_z - view_distance_; z <= current_chunk_z + view_distance_; ++z) {
				std::pair<int, int> chunk_coord = {x, z};
				if (chunk_cache_.find(chunk_coord) == chunk_cache_.end()) {
					chunk_cache_[chunk_coord] = generateChunk(x, z);
				}
			}
		}

		// Unload chunks
		std::vector<std::pair<int, int>> to_remove;
		for (auto const& [key, val] : chunk_cache_) {
			int dx = key.first - current_chunk_x;
			int dz = key.second - current_chunk_z;
			if (std::abs(dx) > view_distance_ || std::abs(dz) > view_distance_) {
				to_remove.push_back(key);
			}
		}

		for (const auto& key : to_remove) {
			chunk_cache_.erase(key);
		}
	}

	float TerrainGenerator::fbm(float x, float z) {
		float total = 0;
		float frequency = frequency_;
		float amplitude = 1.0;
		float max_amplitude = 0;
		for (int i = 0; i < octaves_; i++) {
			total += perlin_noise_.noise2D(x * frequency, z * frequency) * amplitude;
			max_amplitude += amplitude;
			amplitude *= persistence_;
			frequency *= lacunarity_;
		}
		return total / max_amplitude;
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
				float noise = (fbm(worldX, worldZ) + 1.0f) / 2.0f;

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
