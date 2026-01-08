#include "terrain_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <numeric>
#include <ranges>
#include <string>
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

	const std::vector<std::shared_ptr<Terrain>>& TerrainGenerator::getVisibleChunks() const {
		return visible_chunks_;
	}

	auto TerrainGenerator::biomefbm(glm::vec2 pos, BiomeAttributes attr) const {
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

	glm::vec3 TerrainGenerator::pointGenerate(float x, float z) const {
		glm::vec3 path_data = getPathInfluence(x, z);
		float     path_factor = path_data.x;

		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));

		float warp_strength = (1.0f - path_factor) * 20.0f;

		glm::vec2 pos = glm::vec2(x, z);
		glm::vec2 warped_pos = pos + (push_dir * warp_strength);

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

		glm::vec3 terrain_height = biomefbm(warped_pos, current);

		float path_floor_level = -0.10f;
		terrain_height.x = glm::mix(path_floor_level, terrain_height.x, path_factor);

		return terrain_height;
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
			if (pos.y < proxy.minY) {
				proxy.minY = pos.y;
				proxy.lowestPoint = pos;
			}
			if (pos.y > proxy.maxY) {
				proxy.maxY = pos.y;
				proxy.highestPoint = pos;
			}
			float dist_sq = glm::dot(pos - proxy.center, pos - proxy.center);
			if (dist_sq > max_dist_sq) {
				max_dist_sq = dist_sq;
			}
		}
		proxy.radiusSq = max_dist_sq;

		return {indices, positions, normals, proxy, chunkX, chunkZ, true};
	}

	bool
	TerrainGenerator::Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const {
		constexpr float step_size = 1.0f; // Initial step for ray marching
		float           current_dist = 0.0f;
		glm::vec3       current_pos = origin;

		// Ray marching to find a segment that contains the intersection
		while (current_dist < max_dist) {
			current_pos = origin + dir * current_dist;
			float terrain_height = std::get<0>(pointProperties(current_pos.x, current_pos.z));

			if (current_pos.y < terrain_height) {
				// We found an intersection between the previous and current step.
				// Now refine with a binary search.
				float start_dist = std::max(0.0f, current_dist - step_size);
				float end_dist = current_dist;

				constexpr int binary_search_steps = 10; // 10 steps for good precision
				for (int i = 0; i < binary_search_steps; ++i) {
					float     mid_dist = (start_dist + end_dist) / 2.0f;
					glm::vec3 mid_pos = origin + dir * mid_dist;
					float     mid_terrain_height = std::get<0>(pointProperties(mid_pos.x, mid_pos.z));

					if (mid_pos.y < mid_terrain_height) {
						end_dist = mid_dist; // Intersection is in the first half
					} else {
						start_dist = mid_dist; // Intersection is in the second half
					}
				}

				out_dist = (start_dist + end_dist) / 2.0f;
				return true; // Hit
			}

			current_dist += step_size;
		}

		return false; // No hit
	}

	glm::vec3 TerrainGenerator::getPathInfluence(float x, float z) const {
		glm::vec3 noise = Simplex::dnoise(glm::vec2(x, z) * kPathFrequency);
		float     distance_from_spine = std::abs(noise.x);
		float     corridor_width = 0.35f; // Adjust for wider/narrower paths
		float     path_factor = glm::smoothstep(0.0f, corridor_width, distance_from_spine);

		return glm::vec3(path_factor, noise.y, noise.z);
	}

	glm::vec2 TerrainGenerator::findClosestPointOnPath(glm::vec2 sample_pos) const {
		constexpr int   kGradientDescentSteps = 5;
		constexpr float kStepSize = 0.1f;

		for (int i = 0; i < kGradientDescentSteps; ++i) {
			glm::vec3 path_data = Simplex::dnoise(sample_pos * kPathFrequency);
			glm::vec2 gradient = glm::vec2(path_data.y, path_data.z);
			sample_pos -= gradient * path_data.x * kStepSize;
		}

		return sample_pos;
	}

	std::vector<glm::vec3> TerrainGenerator::GetPath(glm::vec2 start_pos, int num_points, float step_size) const {
		std::vector<glm::vec3> path;
		path.reserve(num_points);

		glm::vec2 current_pos = findClosestPointOnPath(start_pos);

		for (int i = 0; i < num_points; ++i) {
			float height = std::get<0>(pointProperties(current_pos.x, current_pos.y));
			path.emplace_back(current_pos.x, height, current_pos.y);

			// Get path tangent
			glm::vec3 path_data = Simplex::dnoise(current_pos * kPathFrequency);
			glm::vec2 tangent = glm::normalize(glm::vec2(path_data.z, -path_data.y));

			// Move along the tangent
			current_pos += tangent * step_size;

			// Correct position to stay on the path
			current_pos = findClosestPointOnPath(current_pos);
		}

		return path;
	}

	std::vector<uint16_t> TerrainGenerator::GenerateSuperChunkTexture(int superChunkX, int superChunkZ) {
		std::filesystem::create_directory("terrain_cache");
		std::string filename = "terrain_cache/superchunk_" + std::to_string(superChunkX) + "_" +
			std::to_string(superChunkZ) + ".dat";
		if (std::filesystem::exists(filename)) {
			std::ifstream infile(filename, std::ios::binary);
			int           width = 0, height = 0;
			infile.read(reinterpret_cast<char*>(&width), sizeof(int));
			infile.read(reinterpret_cast<char*>(&height), sizeof(int));

			// Sanity check the dimensions from the file
			const int kMaxSize = 8192;
			if (width > 0 && height > 0 && width <= kMaxSize && height <= kMaxSize) {
				std::vector<uint16_t> pixels(width * height * 4);
				infile.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(uint16_t));
				infile.close();
				if (infile.gcount() == pixels.size() * sizeof(uint16_t)) {
					logger::LOG("Loaded superchunk from cache: " + filename);
					return pixels;
				}
			}

			// If we're here, the file was invalid.
			logger::LOG("Corrupted superchunk cache file, deleting: " + filename);
			infile.close();
			std::filesystem::remove(filename);
		}

		const int             kSuperChunkSizeInChunks = chunk_size_;
		const int             texture_dim = kSuperChunkSizeInChunks * chunk_size_;
		std::vector<uint16_t> pixels(texture_dim * texture_dim * 4);
		float                 max_height = GetMaxHeight();

		for (int y = 0; y < texture_dim; ++y) {
			for (int x = 0; x < texture_dim; ++x) {
				float worldX = (superChunkX * texture_dim + x);
				float worldZ = (superChunkZ * texture_dim + y);

				auto [height, normal] = pointProperties(worldX, worldZ);

				int index = (y * texture_dim + x) * 4;

				// Normals are in [-1, 1], so map to [0, 65535]
				pixels[index + 0] = static_cast<uint16_t>((normal.x * 0.5f + 0.5f) * 65535.0f);
				pixels[index + 1] = static_cast<uint16_t>((normal.y * 0.5f + 0.5f) * 65535.0f);
				pixels[index + 2] = static_cast<uint16_t>((normal.z * 0.5f + 0.5f) * 65535.0f);

				// Height is in [0, maxHeight], so map to [0, 65535]
				float normalized_height = std::max(0.0f, std::min(1.0f, height / max_height));
				pixels[index + 3] = static_cast<uint16_t>(normalized_height * 65535.0f);
			}
		}

		std::ofstream outfile(filename, std::ios::binary);
		int           width = texture_dim;
		int           height = texture_dim;
		outfile.write(reinterpret_cast<const char*>(&width), sizeof(int));
		outfile.write(reinterpret_cast<const char*>(&height), sizeof(int));
		outfile.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(uint16_t));
		outfile.close();
		return pixels;
	}
} // namespace Boidsish
