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

	auto TerrainGenerator::fbm(float x, float z, TerrainParameters params) {
		glm::vec3 total;
		// float total = 0;
		float frequency = params.frequency;
		float amplitude = 1.0;
		float max_amplitude = 0;
		for (int i = 0; i < octaves_; i++) {
			total += Simplex::dnoise(glm::vec2(x * frequency, z * frequency));
			// total += perlin_noise_.noise2D(x * frequency, z * frequency) * amplitude;
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

		std::vector<std::vector<glm::vec3>> heightmap(num_vertices_x, std::vector<glm::vec3>(num_vertices_z));
		std::vector<float>                  vertexData;
		std::vector<unsigned int>           indices;
		bool                                has_terrain = false;

		// Generate heightmap
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float worldX = (chunkX * chunk_size_ + i);
				float worldZ = (chunkZ * chunk_size_ + j);

				// Get control value to determine biome
				float control_value = control_perlin_noise_
										  .octave2D_01(worldX * control_noise_scale_, worldZ * control_noise_scale_, 2);

				// Interpolate parameters based on control value
				TerrainParameters current_params;
				auto              low_threshold = (floor(control_value * terrain_set.size()) / terrain_set.size());
				auto              high_threshold = (ceil(control_value * terrain_set.size()) / terrain_set.size());
				auto              low_item = terrain_set[int(floor(control_value * terrain_set.size()))];
				auto              high_item = terrain_set[int(ceil(control_value * terrain_set.size()))];
				auto              t = (control_value - low_threshold) / (high_threshold - low_threshold);

				current_params.frequency = std::lerp(low_item.frequency, high_item.frequency, t);
				current_params.amplitude = std::lerp(low_item.amplitude, high_item.amplitude, t);
				current_params.threshold = std::lerp(low_item.threshold, high_item.threshold, t);

				// Simplex::dFlowNoise( position + Simplex::fBm( vec3( position, time * 0.1f ) ), time )
				auto pos = glm::vec2(worldX, worldZ); // * current_params.frequency;
				// auto noise = fbm(worldX, worldZ, current_params);
				// auto noise = siv::dn
				// auto noise = Simplex::dFlowNoise(pos + Simplex::fBm( glm::vec2(pos )), worldZ) * 0.5f + 0.5f;

				// glm::vec3 noise	= glm::vec3( 0.0f );
				// float freq		= 1.0f;
				// float amp		= 0.5f;
				// // float max_amplitude = 0.0f;

				// for( uint8_t i = 0; i < 2; i++ ){
				// 	glm::vec3 n	= Simplex::dnoise( pos * freq );
				// 	noise        += n*amp;
				// 	freq       *= lacunarity_;
				// 	amp        *= persistence_;
				// 	// max_amplitude += amp;
				// }
				// noise /= max_amplitude;

				auto noise = Simplex::dfBm(
					glm::vec2(worldX, worldZ) * current_params.frequency,
					1,
					// 0,
				    // 0
					current_params.frequency,
					current_params.amplitude
				);
				noise = noise * 0.5f + 0.5f;
				if (noise[0] > current_params.threshold) {
					noise[0] -= current_params.threshold;
					noise *= current_params.amplitude;
					heightmap[i][j] = noise;
					has_terrain = true;
				} else {
					heightmap[i][j] = glm::vec3(0.0f, 0, 0); // Flush with floor
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
				float y = heightmap[i][j][0];

				// Vertex position
				vertexData.push_back(i);
				vertexData.push_back(y);
				vertexData.push_back(j);

				glm::vec3 normal = glm::normalize(glm::vec3(-heightmap[i][j][1], 1.0f, -heightmap[i][j][2]));
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
