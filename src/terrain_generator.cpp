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
#include "stb_image_write.h"
#include "zstr.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <libmorton/morton.h>

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

	TerrainGenerator::TerrainGenerator(int seed): seed_(seed), thread_pool_(), eng_(rd_()) {
		Simplex::seed(seed_);

		biome_cdf_.reserve(biomes.size());
		float totalWeight = 0.0f;
		for (const auto& b : biomes) {
			totalWeight += b.weight;
			biome_cdf_.push_back(totalWeight);
		}
	}

	TerrainGenerator::~TerrainGenerator() {
		{
			std::lock_guard<std::mutex> lock(chunk_cache_mutex_);
			for (auto& pair : pending_chunks_) {
				pair.second.cancel();
			}
		}

		// Wait for any in-flight tasks to complete (they may have started before cancel)
		// This prevents use-after-free when tasks reference 'this'
		{
			std::lock_guard<std::mutex> lock(chunk_cache_mutex_);
			for (auto& pair : pending_chunks_) {
				try {
					auto& handle = const_cast<TaskHandle<TerrainGenerationResult>&>(pair.second);
					handle.get(); // Wait for completion, ignore result
				} catch (...) {
					// Ignore exceptions from cancelled/failed tasks
				}
			}
			pending_chunks_.clear();
		}

		{
			std::lock_guard<std::mutex> lock(chunk_cache_mutex_);
			chunk_cache_.clear();
		}
		{
			std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
			visible_chunks_.clear();
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
					{
						std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
						if (chunk_cache_.find(chunk_coord) == chunk_cache_.end() &&
						    pending_chunks_.find(chunk_coord) == pending_chunks_.end()) {
							pending_chunks_.emplace(
								chunk_coord,
								thread_pool_
									.enqueue(TaskPriority::MEDIUM, &TerrainGenerator::generateChunkData, this, x, z)
							);
						}
					}
				}
			}
		}

		// Process completed chunks
		std::vector<std::pair<int, int>> completed_chunks;
		{
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
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
						completed_chunks.push_back(pair.first);
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
			for (const auto& key : completed_chunks) {
				pending_chunks_.erase(key);
			}
		}

		// Unload chunks
		std::vector<std::pair<int, int>> to_remove;
		{
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
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

		std::vector<std::pair<int, int>> to_cancel;
		{
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
			for (auto const& [key, val] : pending_chunks_) {
				int dx = key.first - current_chunk_x;
				int dz = key.second - current_chunk_z;
				if (std::abs(dx) > dynamic_view_distance + kUnloadDistanceBuffer_ ||
				    std::abs(dz) > dynamic_view_distance + kUnloadDistanceBuffer_) {
					to_cancel.push_back(key);
				}
			}
		}

		for (const auto& key : to_cancel) {
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
			pending_chunks_.at(key).cancel();
			pending_chunks_.erase(key);
		}

		{
			std::lock_guard<std::mutex> visible_lock(visible_chunks_mutex_);
			std::lock_guard<std::mutex> cache_lock(chunk_cache_mutex_);
			visible_chunks_.clear();
			for (auto const& [key, val] : chunk_cache_) {
				if (val) {
					visible_chunks_.push_back(val);
				}
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
		std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
		return visible_chunks_;
	}

	std::vector<std::shared_ptr<Terrain>> TerrainGenerator::getVisibleChunksCopy() const {
		std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
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

	void TerrainGenerator::ApplyWeightedBiome(float control_value, BiomeAttributes& current) const {
		if (biomes.empty())
			return;
		if (biomes.size() == 1) {
			current = biomes[0];
			return;
		}

		// Ideally, cache this
		std::vector<float> cdf;
		cdf.reserve(biomes.size());
		float totalWeight = 0.0f;
		for (const auto& b : biomes) {
			totalWeight += b.weight;
			cdf.push_back(totalWeight);
		}

		float target = std::clamp(control_value, 0.0f, 1.0f) * totalWeight;

		auto it = std::upper_bound(cdf.begin(), cdf.end(), target);

		int high_idx = std::distance(cdf.begin(), it);
		int low_idx = std::max(0, high_idx - 1);

		high_idx = std::min(high_idx, (int)biomes.size() - 1);

		float weight_high = cdf[high_idx];
		float weight_low = (high_idx == 0) ? 0.0f : cdf[high_idx - 1];

		float segment_width = weight_high - weight_low;
		float t = (segment_width > 0.0001f) ? (target - weight_low) / segment_width : 0.0f;

		const auto& low_item = biomes[low_idx];
		const auto& high_item = biomes[high_idx];

		current.spikeDamping = std::lerp(low_item.spikeDamping, high_item.spikeDamping, t);
		current.detailMasking = std::lerp(low_item.detailMasking, high_item.detailMasking, t);
		current.floorLevel = std::lerp(low_item.floorLevel, high_item.floorLevel, t);
	}

	TerrainGenerator::BiomeInfo TerrainGenerator::getBiomeInfo(float control_value) const {
		if (biomes.empty())
			return {{0, 0}, 0.0f};
		if (biomes.size() == 1) {
			return {{0, 0}, 0.0f};
		}

		float totalWeight = biome_cdf_.back();
		float target = std::clamp(control_value, 0.0f, 1.0f) * totalWeight;

		auto it = std::upper_bound(biome_cdf_.begin(), biome_cdf_.end(), target);

		int high_idx = std::distance(biome_cdf_.begin(), it);
		high_idx = std::min(high_idx, (int)biomes.size() - 1);

		float weight_high = biome_cdf_[high_idx];
		float weight_low = (high_idx == 0) ? 0.0f : biome_cdf_[high_idx - 1];

		float segment_width = weight_high - weight_low;
		float t_linear = (segment_width > 0.0001f) ? (target - weight_low) / segment_width : 0.0f;

		const float TRANSITION_WIDTH = 0.1f;
		float       blend = 0.0f;
		int         low_idx = high_idx;
		int         high_idx_out = high_idx;

		if (t_linear < TRANSITION_WIDTH && high_idx > 0) {
			low_idx = high_idx - 1;
			blend = glm::smoothstep(0.0f, TRANSITION_WIDTH, t_linear);
		} else if (t_linear > (1.0 - TRANSITION_WIDTH) && high_idx < biomes.size() - 1) {
			low_idx = high_idx;
			high_idx_out = high_idx + 1;
			blend = glm::smoothstep(1.0f - TRANSITION_WIDTH, 1.0f, t_linear);
		}

		return {{low_idx, high_idx_out}, blend};
	}

	std::vector<uint8_t>
	TerrainGenerator::GenerateBiomeDataTexture(int world_x, int world_z, int size) const {
		const int gutter = 1;
		const int new_size = size + gutter * 2;
		std::vector<uint8_t> pixels(new_size * new_size * 4);

		for (int y = 0; y < new_size; ++y) {
			for (int x = 0; x < new_size; ++x) {
				float worldX = (world_x - gutter + x);
				float worldZ = (world_z - gutter + y);

				float     control_value = getBiomeControlValue(worldX, worldZ);
				BiomeInfo biome_info = getBiomeInfo(control_value);

				int index = (y * new_size + x) * 4;

				pixels[index + 0] = static_cast<uint8_t>(biome_info.biome_indices[0]);
				pixels[index + 1] = static_cast<uint8_t>(biome_info.biome_indices[1]);
				pixels[index + 2] = static_cast<uint8_t>(biome_info.blend * 255.0f);
				pixels[index + 3] = 255; // Alpha
			}
		}

		return pixels;
	}

	glm::vec3 TerrainGenerator::pointGenerate(float x, float z) const {
		glm::vec3 path_data = getPathInfluence(x, z);
		float     path_factor = path_data.x;

		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));
		float     warp_strength = (1.0f - path_factor) * 20.0f;
		glm::vec2 warp = push_dir * warp_strength;

		glm::vec2 pos = glm::vec2(x, z);
		glm::vec2 warped_pos = pos + warp;

		float control_value = getBiomeControlValue(x, z);

		if (std::isnan(control_value) || std::isinf(control_value)) {
			control_value = 0.0f;
		}

		BiomeAttributes current;
		ApplyWeightedBiome(control_value, current);

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
			float terrain_height = pointGenerate(current_pos.x, current_pos.z).x;

			if (current_pos.y < terrain_height) {
				// We found an intersection between the previous and current step.
				// Now refine with a binary search.
				float start_dist = std::max(0.0f, current_dist - step_size);
				float end_dist = current_dist;

				constexpr int binary_search_steps = 10; // 10 steps for good precision
				for (int i = 0; i < binary_search_steps; ++i) {
					float     mid_dist = (start_dist + end_dist) / 2.0f;
					glm::vec3 mid_pos = origin + dir * mid_dist;

					float mid_terrain_height = pointGenerate(mid_pos.x, mid_pos.z).x;

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

	std::vector<uint16_t> TerrainGenerator::GenerateSuperChunkTexture(int requested_x, int requested_z) {
		const int kSuperChunkSizeInChunks = chunk_size_;
		const int texture_dim = kSuperChunkSizeInChunks * chunk_size_;

		// Map world coordinates to a consistent grid index
		const int super_chunk_x = floor(static_cast<float>(requested_x) / texture_dim);
		const int super_chunk_z = floor(static_cast<float>(requested_z) / texture_dim);

		std::filesystem::create_directory("terrain_cache");
		const uint64_t morton_code = libmorton::morton2D_64_encode(
			static_cast<uint32_t>(super_chunk_x),
			static_cast<uint32_t>(super_chunk_z)
		);

		std::string    filename = "terrain_cache/superchunk_" + std::to_string(morton_code) + ".dat";
		const uint32_t kMagicNumber = 0x1F9D48E2; // Bump magic number due to format change
		if (std::filesystem::exists(filename)) {
			std::ifstream infile(filename, std::ios::binary);
			if (!infile) {
				logger::LOG("Failed to open superchunk cache file for reading: " + filename);
				std::filesystem::remove(filename);
			} else {
				uint32_t magic_number = 0;
				infile.read(reinterpret_cast<char*>(&magic_number), sizeof(uint32_t));

				if (magic_number == kMagicNumber) {
					// Z-ORDERED COMPRESSED PATH
					int width = 0, height = 0;
					infile.read(reinterpret_cast<char*>(&width), sizeof(int));
					infile.read(reinterpret_cast<char*>(&height), sizeof(int));

					const int kMaxSize = 8192;
					if (width > 0 && height > 0 && width <= kMaxSize && height <= kMaxSize) {
						zstr::istream         z_infile(infile.rdbuf());
						std::vector<uint16_t> z_ordered_pixels(width * height * 4);
						z_infile.read(
							reinterpret_cast<char*>(z_ordered_pixels.data()),
							z_ordered_pixels.size() * sizeof(uint16_t)
						);

						if (z_infile.good()) {
							logger::LOG(
								"Loaded Z-ordered compressed superchunk from cache",
								filename,
								width,
								height,
								z_ordered_pixels.size()
							);
							infile.close();

							// Convert from Z-order to linear
							std::vector<uint16_t> pixels(width * height * 4);
							for (uint32_t y = 0; y < height; ++y) {
								for (uint32_t x = 0; x < width; ++x) {
									uint64_t morton_index = libmorton::morton2D_64_encode(x, y);
									int      linear_index = (y * width + x) * 4;
									memcpy(
										&pixels[linear_index],
										&z_ordered_pixels[morton_index * 4],
										4 * sizeof(uint16_t)
									);
								}
							}
							return pixels;
						}
					}
				} else if (magic_number == 0x1F9D48E1) {
					// LINEAR COMPRESSED (legacy) PATH
					int width = 0, height = 0;
					infile.read(reinterpret_cast<char*>(&width), sizeof(int));
					infile.read(reinterpret_cast<char*>(&height), sizeof(int));

					const int kMaxSize = 8192;
					if (width > 0 && height > 0 && width <= kMaxSize && height <= kMaxSize) {
						zstr::istream         z_infile(infile.rdbuf());
						std::vector<uint16_t> pixels(width * height * 4);
						z_infile.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(uint16_t));

						if (z_infile.good()) {
							logger::LOG(
								"Loaded linear compressed superchunk from cache",
								filename,
								width,
								height,
								pixels.size()
							);
							infile.close();
							return pixels;
						}
					}
				} else {
					// UNCOMPRESSED (very legacy) PATH
					infile.seekg(0);
					int width = 0, height = 0;
					infile.read(reinterpret_cast<char*>(&width), sizeof(int));
					infile.read(reinterpret_cast<char*>(&height), sizeof(int));

					const int kMaxSize = 8192;
					if (width > 0 && height > 0 && width <= kMaxSize && height <= kMaxSize) {
						std::vector<uint16_t> pixels(width * height * 4);
						infile.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(uint16_t));
						if (infile.gcount() == pixels.size() * sizeof(uint16_t)) {
							logger::LOG(
								"Loaded uncompressed superchunk from cache",
								filename,
								width,
								height,
								pixels.size()
							);
							infile.close();
							return pixels;
						}
					}
				}

				// If we reach here, either path failed.
				logger::LOG("Corrupted superchunk cache file, deleting: " + filename);
				infile.close();
				std::filesystem::remove(filename);
			}
		}

		std::vector<uint16_t> pixels(texture_dim * texture_dim * 4);
		float                 max_height = GetMaxHeight();

		for (int y = 0; y < texture_dim; ++y) {
			for (int x = 0; x < texture_dim; ++x) {
				float worldX = (super_chunk_x * texture_dim + x);
				float worldZ = (super_chunk_z * texture_dim + y);

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

		// Convert to Z-order for storage
		std::vector<uint16_t> z_ordered_pixels(texture_dim * texture_dim * 4);
		for (uint32_t y = 0; y < texture_dim; ++y) {
			for (uint32_t x = 0; x < texture_dim; ++x) {
				uint64_t morton_index = libmorton::morton2D_64_encode(x, y);
				int      linear_index = (y * texture_dim + x) * 4;
				memcpy(&z_ordered_pixels[morton_index * 4], &pixels[linear_index], 4 * sizeof(uint16_t));
			}
		}

		std::ofstream outfile(filename, std::ios::binary);
		int           width = texture_dim;
		int           height = texture_dim;
		outfile.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(uint32_t));
		outfile.write(reinterpret_cast<const char*>(&width), sizeof(int));
		outfile.write(reinterpret_cast<const char*>(&height), sizeof(int));
		zstr::ostream z_outfile(outfile.rdbuf());
		z_outfile.write(
			reinterpret_cast<const char*>(z_ordered_pixels.data()),
			z_ordered_pixels.size() * sizeof(uint16_t)
		);
		return pixels;
	}

	float TerrainGenerator::getBiomeControlValue(float x, float z) const {
		glm::vec2 pos(x, z);
		pos *= control_noise_scale_;
		float result = Simplex::noise(pos + Simplex::curlNoise(pos)) * 0.5f + 0.5f;

		return std::clamp(result, 0.0f, 1.0f);

		// return Simplex::worleyfBm(glm::vec2(x * control_noise_scale_, z * control_noise_scale_));
		// float result = Simplex::noise( pos + glm::vec2( Simplex::curlNoise( pos, control_noise_scale_ ).x ) ) * 0.5f
		// + 0.5f; float result = Simplex::worleyfBm(pos+Simplex::curlNoise(pos)) * 0.5f + 0.5f; return
		// Simplex::worleyfBm(pos); return Simplex::worleyNoise(glm::vec2(x * control_noise_scale_, z *
		// control_noise_scale_), 4) * 0.5f + 0.5f;
	}

	glm::vec2 TerrainGenerator::getDomainWarp(float x, float z) const {
		glm::vec3 path_data = getPathInfluence(x, z);
		float     path_factor = path_data.x;
		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));
		float     warp_strength = (1.0f - path_factor) * 20.0f;
		return push_dir * warp_strength;
	}

	void TerrainGenerator::ConvertDatToPng(const std::string& dat_filepath, const std::string& png_filepath) {
		const uint32_t kMagicNumber = 0x1F9D48E1;
		std::ifstream  infile(dat_filepath, std::ios::binary);
		if (!infile) {
			logger::ERROR("Failed to open .dat file: " + dat_filepath);
			return;
		}
		uint32_t magic_number = 0;
		infile.read(reinterpret_cast<char*>(&magic_number), sizeof(uint32_t));

		int                   width, height;
		std::vector<uint16_t> pixels16;

		if (magic_number == kMagicNumber) {
			infile.read(reinterpret_cast<char*>(&width), sizeof(int));
			infile.read(reinterpret_cast<char*>(&height), sizeof(int));
			pixels16.resize(width * height * 4);
			zstr::istream z_infile(infile.rdbuf());
			z_infile.read(reinterpret_cast<char*>(pixels16.data()), pixels16.size() * sizeof(uint16_t));
		} else {
			infile.seekg(0);
			infile.read(reinterpret_cast<char*>(&width), sizeof(int));
			infile.read(reinterpret_cast<char*>(&height), sizeof(int));
			pixels16.resize(width * height * 4);
			infile.read(reinterpret_cast<char*>(pixels16.data()), pixels16.size() * sizeof(uint16_t));
		}
		infile.close();

		std::vector<uint8_t> pixels8(width * height * 4);
		for (size_t i = 0; i < pixels16.size(); ++i) {
			pixels8[i] = static_cast<uint8_t>(pixels16[i] / 256);
		}

		stbi_write_png(png_filepath.c_str(), width, height, 4, pixels8.data(), width * 4);
	}

	std::vector<uint16_t> TerrainGenerator::GenerateTextureForArea(int world_x, int world_z, int size) {
		const int kSuperChunkSizeInChunks = chunk_size_;
		const int texture_dim = kSuperChunkSizeInChunks * chunk_size_;
		const int gutter = 1;
		const int new_size = size + gutter * 2;

		// Determine the range of superchunks needed
		int start_chunk_x = floor(static_cast<float>(world_x - gutter) / texture_dim);
		int end_chunk_x = floor(static_cast<float>(world_x + size + gutter - 1) / texture_dim);
		int start_chunk_z = floor(static_cast<float>(world_z - gutter) / texture_dim);
		int end_chunk_z = floor(static_cast<float>(world_z + size + gutter - 1) / texture_dim);

		// Create the destination texture
		std::vector<uint16_t> stitched_texture(new_size * new_size);

		// Iterate over the needed superchunks and stitch them together
		for (int cz = start_chunk_z; cz <= end_chunk_z; ++cz) {
			for (int cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
				std::vector<uint16_t> superchunk = GenerateSuperChunkTexture(cx * texture_dim, cz * texture_dim);

				// Calculate the region to copy from the superchunk
				int src_start_x = std::max(0, world_x - gutter - cx * texture_dim);
				int src_end_x = std::min(texture_dim, world_x + size + gutter - cx * texture_dim);
				int src_start_z = std::max(0, world_z - gutter - cz * texture_dim);
				int src_end_z = std::min(texture_dim, world_z + size + gutter - cz * texture_dim);

				// Calculate the region to copy to in the destination texture
				int dest_start_x = std::max(0, cx * texture_dim - (world_x - gutter));
				int dest_start_z = std::max(0, cz * texture_dim - (world_z - gutter));

				for (int z = src_start_z; z < src_end_z; ++z) {
					for (int x = src_start_x; x < src_end_x; ++x) {
						int src_index = (z * texture_dim + x) * 4;
						int dest_index = (dest_start_z + z - src_start_z) * new_size + (dest_start_x + x - src_start_x);

						if (dest_index < stitched_texture.size()) {
							stitched_texture[dest_index] = superchunk[src_index + 3];
						}
					}
				}
			}
		}

		return stitched_texture;
	}
} // namespace Boidsish
