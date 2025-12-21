#include "terrain_generator.h"

#include <cmath>
#include <numeric>
#include <vector>

#include "logger.h"
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
				halfSize.y * std::abs(frustum.planes[i].normal.y) + halfSize.z * std::abs(frustum.planes[i].normal.z);
			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
			if (d < -r) {
				return false;
			}
		}
		return true;
	}

	TerrainGenerator::TerrainGenerator(int seed): perlin_noise_(seed), control_perlin_noise_(seed + 1) {}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		float height_factor = std::max(1.0f, camera.y / 5.0f);
		int   dynamic_view_distance = std::min(24, static_cast<int>(view_distance_ * height_factor));

		// Load chunks based on frustum and dynamic view distance
		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				if (isChunkInFrustum(frustum, x, z, chunk_size_, mountains_params_.amplitude)) {
					std::pair<int, int> chunk_coord = {x, z};
					if (chunk_cache_.find(chunk_coord) == chunk_cache_.end()) {
						populateNoiseCache(x, z);
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
			noise_cache_.erase(key);
		}
	}

	void TerrainGenerator::findSurface(
		int chunkX, int chunkZ, std::vector<std::vector<glm::vec3>>& heightmap
	) {
		std::pair<int, int> chunk_coord = {chunkX, chunkZ};
		auto                it = noise_cache_.find(chunk_coord);
		if (it == noise_cache_.end()) {
			return;
		}

		const auto& noise_values = it->second;
		const int   num_vertices_x = chunk_size_ + 1;
		const int   num_vertices_z = chunk_size_ + 1;
		const int   num_vertices_y = chunk_size_ * 2;

		for (int i = 0; i < num_vertices_x; ++i) {
			for (int k = 0; k < num_vertices_z; ++k) {
				for (int j = 1; j < num_vertices_y; ++j) {
					if (noise_values[i][j][k] > 0 && noise_values[i][j - 1][k] <= 0) {
						heightmap[i][k] = glm::vec3(static_cast<float>(j), 0.0f, 0.0f);
						break;
					}
				}
			}
		}
	}

	void TerrainGenerator::populateNoiseCache(int chunkX, int chunkZ) {
		std::pair<int, int> chunk_coord = {chunkX, chunkZ};
		if (noise_cache_.count(chunk_coord)) {
			return;
		}

		const int num_vertices_x = chunk_size_ + 1;
		const int num_vertices_z = chunk_size_ + 1;
		const int num_vertices_y = chunk_size_ * 2;

		std::vector<std::vector<std::vector<float>>> noise_values(
			num_vertices_x,
			std::vector<std::vector<float>>(
				num_vertices_y,
				std::vector<float>(num_vertices_z)
			)
		);

		const float noise_scale = 0.05f;

		for (int i = 0; i < num_vertices_x; ++i) {
			for (int k = 0; k < num_vertices_z; ++k) {
				for (int j = 0; j < num_vertices_y; ++j) {
					float worldX = (chunkX * chunk_size_ + i);
					float worldY = j;
					float worldZ = (chunkZ * chunk_size_ + k);

					float total = 0;
					float frequency = noise_scale;
					float amplitude = 1.0;
					float max_amplitude = 0;
					for (int oct = 0; oct < octaves_; oct++) {
						total += perlin_noise_.noise3D(worldX * frequency, worldY * frequency, worldZ * frequency) *
							amplitude;
						max_amplitude += amplitude;
						amplitude *= persistence_;
						frequency *= lacunarity_;
					}
					float noise_val = total / max_amplitude;

					noise_values[i][j][k] = noise_val;
				}
			}
		}
		noise_cache_[chunk_coord] = noise_values;
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

		std::vector<std::vector<glm::vec3>> heightmap(num_vertices_x, std::vector<glm::vec3>(num_vertices_z, glm::vec3(-1.0f)));
		std::vector<float>                  vertexData;
		std::vector<unsigned int>           indices;
		bool                                has_terrain = false;

		findSurface(chunkX, chunkZ, heightmap);
		// Generate heightmap
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				if (heightmap[i][j].x > 0) {
					has_terrain = true;
					break;
				}
			}
		}

		if (!has_terrain) {
			return nullptr;
		}

		// Generate vertices and normals
		vertexData.reserve(num_vertices_x * num_vertices_z * 8);
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float y = heightmap[i][j].x;

				// Vertex position
				vertexData.push_back(i);
				vertexData.push_back(y);
				vertexData.push_back(j);

				// Calculate normals
				float heightL = (i > 0) ? heightmap[i - 1][j].x : y;
				float heightR = (i < num_vertices_x - 1) ? heightmap[i + 1][j].x : y;
				float heightD = (j > 0) ? heightmap[i][j - 1].x : y;
				float heightU = (j < num_vertices_z - 1) ? heightmap[i][j + 1].x : y;

				glm::vec3 normal(heightL - heightR, 2.0f, heightD - heightU);
				normal = glm::normalize(normal);

				vertexData.push_back(normal.x);
				vertexData.push_back(normal.y);
				vertexData.push_back(normal.z);

				// Texture coordinates
				vertexData.push_back((float)i / chunk_size_);
				vertexData.push_back((float)j / chunk_size_);
			}
		}

		// Generate indices for a grid of quads
		indices.reserve(chunk_size_ * chunk_size_ * 4);
		for (int i = 0; i < chunk_size_; ++i) {
			for (int j = 0; j < chunk_size_; ++j) {
				indices.push_back(i * num_vertices_z + j);
				indices.push_back((i + 1) * num_vertices_z + j);
				indices.push_back((i + 1) * num_vertices_z + j + 1);
				indices.push_back(i * num_vertices_z + j + 1);
			}
		}

		auto terrain_chunk = std::make_shared<Terrain>(vertexData, indices);
		terrain_chunk->SetPosition(chunkX * chunk_size_, 0, chunkZ * chunk_size_);

		return terrain_chunk;
	}

} // namespace Boidsish
