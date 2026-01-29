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
	}

	TerrainGenerator::~TerrainGenerator() {
		{
			for (auto& pair : pending_chunks_) {
				pair.second.cancel();
			}
		}

		// Wait for any in-flight tasks to complete (they may have started before cancel)
		// This prevents use-after-free when tasks reference 'this'
		{
			for (auto& pair : pending_chunks_) {
				try {
					auto& handle = const_cast<TaskHandle<TerrainGenerationResult>&>(pair.second);
					handle.get(); // Wait for completion, ignore result
				} catch (...) {
					// Ignore exceptions from cancelled/failed tasks
				}
			}
			pending_chunks_.clear();

			std::lock_guard<std::mutex> lock(superchunk_mutex_);
			for (auto& pair : pending_superchunk_tasks_) {
				try {
					pair.second.get();
				} catch (...) {
				}
			}
			pending_superchunk_tasks_.clear();
		}

		{
			chunk_cache_.clear();
		}
		{
			std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
			visible_chunks_.clear();
		}
	}

	void TerrainGenerator::update(const Frustum& frustum, const Camera& camera) {
		// Use floor division for correct negative coordinate handling
		int current_chunk_x = static_cast<int>(std::floor(camera.x / static_cast<float>(chunk_size_)));
		int current_chunk_z = static_cast<int>(std::floor(camera.z / static_cast<float>(chunk_size_)));

		float height_factor = std::max(1.0f, camera.y / 5.0f);
		int   dynamic_view_distance = std::min(Constants::Class::Terrain::MaxViewDistance(), static_cast<int>(view_distance_ * height_factor));

		float max_h = 0.0f;
		for (const auto& b : biomes) {
			max_h = std::max(max_h, b.floorLevel);
		}

		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);

		// 1. Enqueue generation of new chunks within view distance
		// Priority is HIGH for chunks in the frustum, and LOW for those outside
		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				std::pair<int, int> chunk_coord = {x, z};
				if (chunk_cache_.find(chunk_coord) == chunk_cache_.end() &&
				    pending_chunks_.find(chunk_coord) == pending_chunks_.end()) {
					bool         in_frustum = isChunkInFrustum(frustum, x, z, chunk_size_, max_h);
					TaskPriority priority = in_frustum ? TaskPriority::HIGH : TaskPriority::LOW;

					pending_chunks_.emplace(
						chunk_coord,
						thread_pool_.enqueue(priority, &TerrainGenerator::generateChunkData, this, x, z)
					);
				}
			}
		}

		// 2. Process completed chunks (without blocking the main thread)
		std::vector<std::pair<int, int>> completed_chunks;
		for (auto& pair : pending_chunks_) {
			if (pair.second.is_ready()) {
				try {
					auto&                   future = const_cast<TaskHandle<TerrainGenerationResult>&>(pair.second);
					TerrainGenerationResult result = future.get();
					if (result.has_terrain) {
						auto terrain_chunk =
							std::make_shared<Terrain>(result.indices, result.positions, result.normals, result.proxy);
						terrain_chunk->SetPosition(result.chunk_x * chunk_size_, 0, result.chunk_z * chunk_size_);

						if (render_manager_) {
							terrain_chunk->SetManagedByRenderManager(true);
							// Registration is deferred to the registration pass below
						} else {
							terrain_chunk->setupMesh();
						}

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

		for (const auto& key : completed_chunks) {
			pending_chunks_.erase(key);
		}

		// 2.5 Clean up completed superchunk tasks
		{
			std::lock_guard<std::mutex>      lock(superchunk_mutex_);
			std::vector<uint64_t> completed_superchunks;
			for (auto& pair : pending_superchunk_tasks_) {
				if (pair.second.is_ready()) {
					completed_superchunks.push_back(pair.first);
				}
			}
			for (auto morton : completed_superchunks) {
				pending_superchunk_tasks_.erase(morton);
			}
		}

		// 3. Registration Pass: Register chunks that are in frustum but not in renderer
		if (render_manager_) {
			int       registrations_this_frame = 0;
			const int max_registrations_per_frame = 10;

			for (auto const& [key, terrain_chunk] : chunk_cache_) {
				if (registrations_this_frame >= max_registrations_per_frame)
					break;

				if (isChunkInFrustum(frustum, key.first, key.second, chunk_size_, max_h)) {
					if (!render_manager_->HasChunk(key)) {
						render_manager_->RegisterChunk(
							key,
							terrain_chunk->vertices,
							terrain_chunk->normals,
							terrain_chunk->GetIndices(),
							terrain_chunk->proxy.minY,
							terrain_chunk->proxy.maxY,
							glm::vec3(key.first * chunk_size_, 0, key.second * chunk_size_)
						);
						registrations_this_frame++;
					}
				}
			}
		}

		// 4. Unload chunks
		std::vector<std::pair<int, int>> to_remove;
		for (auto const& [key, val] : chunk_cache_) {
			int dx = std::abs(key.first - current_chunk_x);
			int dz = std::abs(key.second - current_chunk_z);
			int limit = dynamic_view_distance + kUnloadDistanceBuffer_;
			if (dx > limit || dz > limit) {
				to_remove.push_back(key);
			}
		}

		for (const auto& key : to_remove) {
			if (render_manager_) {
				render_manager_->UnregisterChunk(key);
			}
			chunk_cache_.erase(key);
		}

		std::vector<std::pair<int, int>> to_cancel;
		for (auto const& [key, val] : pending_chunks_) {
			int dx = std::abs(key.first - current_chunk_x);
			int dz = std::abs(key.second - current_chunk_z);
			int limit = dynamic_view_distance + kUnloadDistanceBuffer_;
			if (dx > limit || dz > limit) {
				to_cancel.push_back(key);
			}
		}

		for (const auto& key : to_cancel) {
			pending_chunks_.at(key).cancel();
			pending_chunks_.erase(key);
		}

		// Commit any pending buffer updates to the render manager
		if (render_manager_) {
			render_manager_->CommitUpdates();
		}

		{
			std::lock_guard<std::mutex> visible_lock(visible_chunks_mutex_);
			visible_chunks_.clear();
			for (auto const& [key, val] : chunk_cache_) {
				if (val && isChunkInFrustum(frustum, key.first, key.second, chunk_size_, max_h)) {
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
		// Account for frequency in analytical derivatives
		base.y *= freq;
		base.z *= freq;
		height = base * amp;

		for (int i = 1; i < 6; i++) {
			amp *= 0.5f;
			freq *= 2.0f;
			glm::vec3 n = Simplex::dnoise(pos * freq);
			n.y *= freq;
			n.z *= freq;

			// 1. Spikiness Correction using Biome Attribute
			float slope = glm::length(glm::vec2(n.y, n.z));
			float correction = 1.0f / (1.0f + slope * attr.spikeDamping);

			// 2. Detail Masking (Valleys stay smoother than peaks)
			auto mask = glm::mix(1.0f - attr.detailMasking, 1.0f, height.x);
			height += (n * amp * correction * mask);
		}

		// 3. Final Floor Shaping
		if (height.x < attr.floorLevel) {
			float t = glm::smoothstep(attr.floorLevel - 0.1f, attr.floorLevel, height.x);
			height.x = t * attr.floorLevel;
			height.y *= t;
			height.z *= t;
		}

		height.x = height.x * 0.5f + 0.5f;
		height.y = height.y * 0.5f;
		height.z = height.z * 0.5f;

		if (height.x > 0) {
			float floorScale = attr.floorLevel;
			height.x *= floorScale;
			// Dampen normal steepness slightly to prevent extreme lighting artifacts in depressions
			// while keeping the visual height the same. 0.4f provides a good balance.
			float normalScale = floorScale;// * 0.4f;
			height.y *= normalScale;
			height.z *= normalScale;
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

	glm::vec3 TerrainGenerator::pointGenerate(float x, float z) const {
		glm::vec3 path_data = getPathInfluence(x, z);
		float     path_factor = path_data.x;

		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));
		float     warp_strength = (1.0f - path_factor) * Constants::Class::Terrain::WarpStrength();
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

		auto      info = getSuperChunkInfo(chunkX * chunk_size_, chunkZ * chunk_size_);
		bool      use_superchunk = false;
		float     max_height = GetMaxHeight();
		const int texture_dim = chunk_size_ * chunk_size_;

		std::vector<uint16_t> pixels;
		if (std::filesystem::exists(info.filename)) {
			pixels = GenerateSuperChunkTexture(chunkX * chunk_size_, chunkZ * chunk_size_);
			if (pixels.size() == texture_dim * texture_dim * 4) {
				use_superchunk = true;
			}
		} else {
			// Dispatch superchunk generation if not already pending
			std::lock_guard<std::mutex> lock(superchunk_mutex_);
			if (pending_superchunk_tasks_.find(info.morton) == pending_superchunk_tasks_.end()) {
				pending_superchunk_tasks_.emplace(
					info.morton,
					thread_pool_.enqueue(
						TaskPriority::LOW,
						&TerrainGenerator::GenerateSuperChunkTexture,
						this,
						chunkX * chunk_size_,
						chunkZ * chunk_size_
					)
				);
			}
		}

		// Generate heightmap
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float worldX = (chunkX * chunk_size_ + i);
				float worldZ = (chunkZ * chunk_size_ + j);

				if (use_superchunk) {
					int relX = static_cast<int>(std::floor(worldX)) - info.super_x * texture_dim;
					int relZ = static_cast<int>(std::floor(worldZ)) - info.super_z * texture_dim;

					if (relX >= 0 && relX < texture_dim && relZ >= 0 && relZ < texture_dim) {
						int       index = (relZ * texture_dim + relX) * 4;
						float     h = (static_cast<float>(pixels[index + 0]) / 65535.0f) * max_height;
						glm::vec3 n;
						n.x = (static_cast<float>(pixels[index + 1]) / 65535.0f) * 2.0f - 1.0f;
						n.y = (static_cast<float>(pixels[index + 2]) / 65535.0f) * 2.0f - 1.0f;
						n.z = (static_cast<float>(pixels[index + 3]) / 65535.0f) * 2.0f - 1.0f;

						// Reconstruct dx, dz for diffToNorm
						float dx = -n.x / std::max(0.0001f, n.y);
						float dz = -n.z / std::max(0.0001f, n.y);

						heightmap[i][j] = glm::vec3(h, dx, dz);
						has_terrain = has_terrain || h > 0;
						continue;
					}
				}

				auto noise = pointGenerate(worldX, worldZ);
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
		float     corridor_width = Constants::Class::Terrain::PathCorridorWidth(); // Adjust for wider/narrower paths
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

	TerrainGenerator::SuperChunkInfo TerrainGenerator::getSuperChunkInfo(int world_x, int world_z) const {
		const int texture_dim = chunk_size_ * chunk_size_;
		const int super_x = floor(static_cast<float>(world_x) / texture_dim);
		const int super_z = floor(static_cast<float>(world_z) / texture_dim);

		const uint64_t morton_code = libmorton::morton2D_64_encode(
			static_cast<uint32_t>(super_x),
			static_cast<uint32_t>(super_z)
		);

		return {
			morton_code,
			"terrain_cache/superchunk_" + std::to_string(morton_code) + ".dat",
			super_x,
			super_z};
	}

	std::vector<uint16_t> TerrainGenerator::GenerateSuperChunkTexture(int requested_x, int requested_z) {
		const int texture_dim = chunk_size_ * chunk_size_;
		auto      info = getSuperChunkInfo(requested_x, requested_z);

		std::filesystem::create_directory("terrain_cache");
		const uint64_t morton_code = info.morton;
		std::string    filename = info.filename;

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
				float worldX = (info.super_x * texture_dim + x);
				float worldZ = (info.super_z * texture_dim + y);

				auto [height, normal] = pointProperties(worldX, worldZ);

				int index = (y * texture_dim + x) * 4;

				// Height is in [0, maxHeight], so map to [0, 65535]
				// This MUST be the first component (R) as expected by shaders
				float normalized_height = std::max(0.0f, std::min(1.0f, height / max_height));
				pixels[index + 0] = static_cast<uint16_t>(normalized_height * 65535.0f);

				// Normals are in [-1, 1], so map to [0, 65535]
				// These are the next three components (GBA)
				pixels[index + 1] = static_cast<uint16_t>((normal.x * 0.5f + 0.5f) * 65535.0f);
				pixels[index + 2] = static_cast<uint16_t>((normal.y * 0.5f + 0.5f) * 65535.0f);
				pixels[index + 3] = static_cast<uint16_t>((normal.z * 0.5f + 0.5f) * 65535.0f);
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

		std::string   temp_filename = filename + ".tmp";
		std::ofstream outfile(temp_filename, std::ios::binary);
		int           width = texture_dim;
		int           height = texture_dim;
		outfile.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(uint32_t));
		outfile.write(reinterpret_cast<const char*>(&width), sizeof(int));
		outfile.write(reinterpret_cast<const char*>(&height), sizeof(int));
		{
			zstr::ostream z_outfile(outfile.rdbuf());
			z_outfile.write(
				reinterpret_cast<const char*>(z_ordered_pixels.data()),
				z_ordered_pixels.size() * sizeof(uint16_t)
			);
		}
		outfile.close();
		std::filesystem::rename(temp_filename, filename);

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

		// Determine the range of superchunks needed
		int start_chunk_x = floor(static_cast<float>(world_x) / texture_dim);
		int end_chunk_x = floor(static_cast<float>(world_x + size - 1) / texture_dim);
		int start_chunk_z = floor(static_cast<float>(world_z) / texture_dim);
		int end_chunk_z = floor(static_cast<float>(world_z + size - 1) / texture_dim);

		// Create the destination texture
		std::vector<uint16_t> stitched_texture(size * size * 4);

		// Iterate over the needed superchunks and stitch them together
		for (int cz = start_chunk_z; cz <= end_chunk_z; ++cz) {
			for (int cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
				std::vector<uint16_t> superchunk = GenerateSuperChunkTexture(cx * texture_dim, cz * texture_dim);

				// Calculate the region to copy from the superchunk
				int src_start_x = std::max(0, world_x - cx * texture_dim);
				int src_end_x = std::min(texture_dim, world_x + size - cx * texture_dim);
				int src_start_z = std::max(0, world_z - cz * texture_dim);
				int src_end_z = std::min(texture_dim, world_z + size - cz * texture_dim);

				// Calculate the region to copy to in the destination texture
				int dest_start_x = std::max(0, cx * texture_dim - world_x);
				int dest_start_z = std::max(0, cz * texture_dim - world_z);

				for (int z = src_start_z; z < src_end_z; ++z) {
					for (int x = src_start_x; x < src_end_x; ++x) {
						int src_index = (z * texture_dim + x) * 4;
						int dest_index = ((dest_start_z + z - src_start_z) * size + (dest_start_x + x - src_start_x)) *
							4;

						if (dest_index + 3 < stitched_texture.size()) {
							stitched_texture[dest_index + 0] = superchunk[src_index + 0];
							stitched_texture[dest_index + 1] = superchunk[src_index + 1];
							stitched_texture[dest_index + 2] = superchunk[src_index + 2];
							stitched_texture[dest_index + 3] = superchunk[src_index + 3];
						}
					}
				}
			}
		}

		return stitched_texture;
	}
} // namespace Boidsish
