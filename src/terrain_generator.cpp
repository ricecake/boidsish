#include "terrain_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <numeric>
#include <ranges>
#include <vector>

#include "graphics.h"
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

	TerrainGenerator::TerrainGenerator(int seed): control_perlin_noise_(seed + 1), thread_pool_() {}

	TerrainGenerator::~TerrainGenerator() {
		for (auto& pair : pending_chunks_) {
			pair.second.cancel();
		}
	}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		int current_chunk_x = static_cast<int>(camera.x) / chunk_size_;
		int current_chunk_z = static_cast<int>(camera.z) / chunk_size_;

		float height_factor = std::max(1.0f, camera.y / 5.0f);
		int   dynamic_view_distance = std::min(24, static_cast<int>(view_distance_ * height_factor));

		// Enqueue generation of new chunks
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
					if (chunk_cache_.find(chunk_coord) == chunk_cache_.end() &&
					    pending_chunks_.find(chunk_coord) == pending_chunks_.end()) {
						pending_chunks_.emplace(
							chunk_coord,
							thread_pool_.enqueue(TaskPriority::MEDIUM, &TerrainGenerator::generateChunkData, this, x, z)
						);
					}
				}
			}
		}

		// Process completed chunks
		std::vector<std::pair<int, int>> completed_chunks;
		for (auto& pair : pending_chunks_) {
			try {
				auto&                   future = const_cast<TaskHandle<TerrainGenerationResult>&>(pair.second);
				TerrainGenerationResult result = future.get();
				if (result.has_terrain) {
					auto terrain_chunk =
						std::make_shared<Terrain>(result.indices, result.positions, result.normals, result.proxy);
					terrain_chunk->SetPosition(result.chunk_x * chunk_size_, 0, result.chunk_z * chunk_size_);
					terrain_chunk->setupMesh();
					chunk_cache_[pair.first] = terrain_chunk;
				}
				completed_chunks.push_back(pair.first);
			} catch (const std::future_error& e) {
				if (e.code() == std::future_errc::no_state) {
					// Task was cancelled, remove it.
					completed_chunks.push_back(pair.first);
				}
			}
		}

		for (const auto& key : completed_chunks) {
			pending_chunks_.erase(key);
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

		std::vector<std::pair<int, int>> to_cancel;
		for (auto const& [key, val] : pending_chunks_) {
			int dx = key.first - current_chunk_x;
			int dz = key.second - current_chunk_z;
			if (std::abs(dx) > dynamic_view_distance + kUnloadDistanceBuffer_ ||
			    std::abs(dz) > dynamic_view_distance + kUnloadDistanceBuffer_) {
				to_cancel.push_back(key);
			}
		}

		for (const auto& key : to_cancel) {
			pending_chunks_.at(key).cancel();
			pending_chunks_.erase(key);
		}

		visible_chunks_.clear();
		for (auto const& [key, val] : chunk_cache_) {
			if (val) {
				visible_chunks_.push_back(val);
			}
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

	const std::vector<std::shared_ptr<Terrain>>& TerrainGenerator::getVisibleChunks() {
		return visible_chunks_;
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

		height = height * 0.5f + 0.5f;

		if (height[0] > 0) {
			height *= attr.floorLevel;
		}

		return height;
	};

	glm::vec3 TerrainGenerator::pointGenerate(float x, float z) {
		// Get control value to determine biome
		float control_value = control_perlin_noise_.octave2D_01(x * control_noise_scale_, z * control_noise_scale_, 2);

		BiomeAttributes current;
		auto            low_threshold = (floor(control_value * biomes.size()) / biomes.size());
		auto            high_threshold = (ceil(control_value * biomes.size()) / biomes.size());
		auto            low_item = biomes[int(floor(control_value * biomes.size()))];
		auto            high_item = biomes[int(ceil(control_value * biomes.size()))];
		auto            t = glm::smoothstep(low_threshold, high_threshold, control_value);

		current.spikeDamping = std::lerp(low_item.spikeDamping, high_item.spikeDamping, t);
		current.detailMasking = std::lerp(low_item.detailMasking, high_item.detailMasking, t);
		current.floorLevel = std::lerp(low_item.floorLevel, high_item.floorLevel, t);

		auto pos = glm::vec2(x, z);
		return biomefbm(pos, current);
	}

	TerrainGenerationResult TerrainGenerator::generateChunkData(int chunkX, int chunkZ) {
		const int num_vertices_x = chunk_size_ + 1;
		const int num_vertices_z = chunk_size_ + 1;

		std::vector<std::vector<glm::vec3>> heightmap(num_vertices_x, std::vector<glm::vec3>(num_vertices_z));
		std::vector<glm::vec3>              positions;
		std::vector<glm::vec3>              normals;
		std::vector<unsigned int>           indices;
		bool                                has_terrain = false;

		// Generate heightmap
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float worldX = (chunkX * chunk_size_ + i);
				float worldZ = (chunkZ * chunk_size_ + j);
				auto  noise = pointGenerate(worldX, worldZ);
				heightmap[i][j] = noise;
				has_terrain = has_terrain || noise[0] > 0;
			}
		}

		if (!has_terrain) {
			return {{}, {}, {}, {}, chunkX, chunkZ, false};
		}

		// Generate vertices and normals
		positions.reserve(num_vertices_x * num_vertices_z);
		normals.reserve(num_vertices_x * num_vertices_z);
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float y = heightmap[i][j][0];
				positions.emplace_back(i, y, j);
				normals.push_back(diffToNorm(heightmap[i][j][1], heightmap[i][j][2]));
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

		// Calculate aggregate data for the PatchProxy
		PatchProxy proxy;
		proxy.center = std::accumulate(positions.begin(), positions.end(), glm::vec3(0.0f)) / (float)positions.size();
		proxy.totalNormal = std::accumulate(normals.begin(), normals.end(), glm::vec3(0.0f));

		float max_dist_sq = 0.0f;
		proxy.minY = std::numeric_limits<float>::max();
		proxy.maxY = std::numeric_limits<float>::lowest();

		for (const auto& pos : positions) {
			proxy.minY = std::min(proxy.minY, pos.y);
			proxy.maxY = std::max(proxy.maxY, pos.y);
			float dist_sq = glm::dot(pos - proxy.center, pos - proxy.center);
			if (dist_sq > max_dist_sq) {
				max_dist_sq = dist_sq;
			}
		}
		proxy.radiusSq = max_dist_sq;

		return {indices, positions, normals, proxy, chunkX, chunkZ, true};
	}

} // namespace Boidsish
