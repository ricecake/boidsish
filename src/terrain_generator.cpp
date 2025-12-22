#include "terrain_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <ranges>
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

	TerrainGenerator::TerrainGenerator(int seed): control_perlin_noise_(seed + 1) {}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		float height_factor = std::max(1.0f, camera.y / 5.0f);
		int   dynamic_view_distance = std::min(24, static_cast<int>(view_distance_ * height_factor));

		// Load chunks based on frustum and dynamic view distance
		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				if (isChunkInFrustum(
						frustum,
						x,
						z,
						chunk_size_,
						std::ranges::max(std::views::transform(biomes, &BiomeAttributes::floorLevel))
					)) {
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
			if (std::abs(dx) > dynamic_view_distance + kUnloadDistanceBuffer_ ||
			    std::abs(dz) > dynamic_view_distance + kUnloadDistanceBuffer_) {
				to_remove.push_back(key);
			}
		}

		for (const auto& key : to_remove) {
			chunk_cache_.erase(key);
		}
	}

	auto TerrainGenerator::fbm(float x, float z, TerrainParameters params) {
		glm::vec3 total;
		float     frequency = params.frequency;
		float     amplitude = 1.0;
		float     max_amplitude = 0;
		for (int i = 0; i < octaves_; i++) {
			total += Simplex::dnoise(glm::vec2(x * frequency, z * frequency));
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

	auto TerrainGenerator::biomefbm(glm::vec2 pos, BiomeAttributes attr) {
		glm::vec3 height(0, 0, 0);
		float     amp = 0.5f;
		float     freq = 0.99f;

		// Initial low-frequency pass to establish "Base Shape"
		glm::vec3 base = Simplex::dnoise(pos * freq);
		height = base * amp;

		for (int i = 1; i < 6; i++) {
			amp *= 0.5f;
			freq *= 2.0f;
			glm::vec3 n = Simplex::dnoise(pos * freq);

			// 1. Spikiness Correction using Biome Attribute
			float slope = glm::length(glm::vec2(n.y, n.z));
			float correction = 1.0f / (1.0f + slope * attr.spikeDamping);

			// 2. Detail Masking (Valleys stay smoother than peaks)
			auto mask = glm::mix(1.0f - attr.detailMasking, 1.0f, height.x);
			height += (n * amp * correction * mask);
		}

		// 3. Final Floor Shaping
		if (height.x < attr.floorLevel) {
			height = glm::smoothstep(attr.floorLevel - 0.1f, attr.floorLevel, height) * attr.floorLevel;
		}

		return height;
	};

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

				BiomeAttributes current;
				auto            low_threshold = (floor(control_value * biomes.size()) / biomes.size());
				auto            high_threshold = (ceil(control_value * biomes.size()) / biomes.size());
				auto            low_item = biomes[int(floor(control_value * biomes.size()))];
				auto            high_item = biomes[int(ceil(control_value * biomes.size()))];
				auto            t = glm::smoothstep(low_threshold, high_threshold, control_value);

				current.spikeDamping = std::lerp(low_item.spikeDamping, high_item.spikeDamping, t);
				current.detailMasking = std::lerp(low_item.detailMasking, high_item.detailMasking, t);
				current.floorLevel = std::lerp(low_item.floorLevel, high_item.floorLevel, t);

				auto pos = glm::vec2(worldX, worldZ);
				auto noise = biomefbm(pos, current);
				noise = noise * 0.5f + 0.5f;

				if (noise[0] > 0) {
					noise *= current.floorLevel;
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
