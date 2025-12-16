#include "terrain_generator.h"

#include <cmath>
#include <numeric>
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

	TerrainGenerator::TerrainGenerator(int seed)
		: perlin_noise_(seed), control_perlin_noise_(seed + 1) {}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		float height_factor = std::max(1.0f, camera.y / 10.0f);
		int   dynamic_view_distance = std::min(24, static_cast<int>(view_distance_ * height_factor));

		// Load chunks based on frustum and dynamic view distance
		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				if (isChunkInFrustum(frustum, x, z, chunk_size_, mountains_params_.amplitude)) {
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

	float TerrainGenerator::fbm(float x, float z, TerrainParameters params) {
		float total = 0;
		float frequency = params.frequency;
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

				// Get control value to determine biome
				float control_value = control_perlin_noise_.octave2D_01(
				    worldX * control_noise_scale_, worldZ * control_noise_scale_, 2);

				// Interpolate parameters based on control value
				TerrainParameters current_params;
				if (control_value < hills_threshold_) {
					// Interpolate between plains and hills
					float t = control_value / hills_threshold_;
					current_params.frequency = std::lerp(plains_params_.frequency, hills_params_.frequency, t);
					current_params.amplitude =
					    std::lerp(plains_params_.amplitude, hills_params_.amplitude, t);
					current_params.threshold =
					    std::lerp(plains_params_.threshold, hills_params_.threshold, t);
				} else {
					// Interpolate between hills and mountains
					float t = (control_value - hills_threshold_) / (1.0f - hills_threshold_);
					current_params.frequency =
					    std::lerp(hills_params_.frequency, mountains_params_.frequency, t);
					current_params.amplitude =
					    std::lerp(hills_params_.amplitude, mountains_params_.amplitude, t);
					current_params.threshold =
					    std::lerp(hills_params_.threshold, mountains_params_.threshold, t);
				}


				float noise = (fbm(worldX, worldZ, current_params) + 1.0f) / 2.0f;
				if (noise > current_params.threshold) {
					heightmap[i][j] = (noise - current_params.threshold) * current_params.amplitude;
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
		vertexData.reserve(num_vertices_x * num_vertices_z * 8);
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
