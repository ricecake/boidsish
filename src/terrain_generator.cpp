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
#include "terrain_deformations.h"
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
		}

		{
			chunk_cache_.clear();
		}
		{
			std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
			visible_chunks_.clear();
		}
	}

	void TerrainGenerator::Update(const Frustum& frustum, const Camera& camera) {
		float scaled_chunk_size = static_cast<float>(chunk_size_) * world_scale_;

		// Use floor division for correct negative coordinate handling
		int current_chunk_x = static_cast<int>(std::floor(camera.x / scaled_chunk_size));
		int current_chunk_z = static_cast<int>(std::floor(camera.z / scaled_chunk_size));

		float height_factor = std::max(1.0f, camera.y / 5.0f);
		int   dynamic_view_distance = std::min(
            Constants::Class::Terrain::MaxViewDistance(),
            static_cast<int>(view_distance_ * height_factor)
        );

		float max_h = 0.0f;
		for (const auto& b : biomes) {
			max_h = std::max(max_h, b.floorLevel);
		}

		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);

		// 1. Collect chunks to enqueue with priority based on distance and frustum visibility
		struct ChunkToEnqueue {
			int          x, z;
			TaskPriority priority;
			float        distance_sq;
		};

		std::vector<ChunkToEnqueue> chunks_to_enqueue;
		chunks_to_enqueue.reserve((2 * dynamic_view_distance + 1) * (2 * dynamic_view_distance + 1));

		glm::vec2 camera_pos_2d(camera.x, camera.z);

		for (int x = current_chunk_x - dynamic_view_distance; x <= current_chunk_x + dynamic_view_distance; ++x) {
			for (int z = current_chunk_z - dynamic_view_distance; z <= current_chunk_z + dynamic_view_distance; ++z) {
				std::pair<int, int> chunk_coord = {x, z};
				if (chunk_cache_.find(chunk_coord) == chunk_cache_.end() &&
				    pending_chunks_.find(chunk_coord) == pending_chunks_.end()) {
					bool in_frustum = isChunkInFrustum(frustum, x, z, scaled_chunk_size, max_h);

					// Calculate distance from chunk center to camera
					glm::vec2 chunk_center(
						x * scaled_chunk_size + scaled_chunk_size * 0.5f,
						z * scaled_chunk_size + scaled_chunk_size * 0.5f
					);
					float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);

					// Priority: HIGH for in-frustum chunks, LOW for out-of-frustum
					// Within each priority level, distance will determine order
					TaskPriority priority = in_frustum ? TaskPriority::HIGH : TaskPriority::LOW;

					chunks_to_enqueue.push_back({x, z, priority, dist_sq});
				}
			}
		}

		// Sort by priority (HIGH first), then by distance within each priority level
		std::sort(
			chunks_to_enqueue.begin(),
			chunks_to_enqueue.end(),
			[](const ChunkToEnqueue& a, const ChunkToEnqueue& b) {
				if (a.priority != b.priority) {
					return a.priority > b.priority; // HIGH (1) before LOW (0)
				}
				return a.distance_sq < b.distance_sq;
			}
		);

		// Enqueue in sorted order
		for (const auto& chunk : chunks_to_enqueue) {
			std::pair<int, int> chunk_coord = {chunk.x, chunk.z};
			pending_chunks_.emplace(
				chunk_coord,
				thread_pool_.enqueue(chunk.priority, &TerrainGenerator::generateChunkData, this, chunk.x, chunk.z)
			);
		}

		// 2. Process completed chunks (without blocking the main thread)
		std::vector<std::pair<int, int>> completed_chunks;
		for (auto& pair : pending_chunks_) {
			if (pair.second.is_ready()) {
				try {
					auto&                   future = const_cast<TaskHandle<TerrainGenerationResult>&>(pair.second);
					TerrainGenerationResult result = future.get();
					if (result.has_terrain) {
						auto terrain_chunk = std::make_shared<Terrain>(
							result.indices,
							result.positions,
							result.normals,
							result.proxy,
							result.occluders
						);
						terrain_chunk
							->SetPosition(result.chunk_x * scaled_chunk_size, 0, result.chunk_z * scaled_chunk_size);

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

		// 3. Registration Pass: Register chunks with render manager
		// Priority: 1) In frustum and close, 2) In frustum and far, 3) Out of frustum but close
		// This prevents pop-in by pre-registering nearby chunks even if not visible yet
		if (render_manager_) {
			const int   max_registrations_per_frame = 32; // Increased for faster catch-up
			const float preload_distance_sq = (dynamic_view_distance * scaled_chunk_size * 0.5f) *
				(dynamic_view_distance * scaled_chunk_size * 0.5f);

			// Collect chunks needing registration with their distances and frustum status
			struct ChunkToRegister {
				std::pair<int, int>      key;
				std::shared_ptr<Terrain> terrain;
				float                    distance_sq;
				bool                     in_frustum;
			};

			std::vector<ChunkToRegister> chunks_to_register;
			chunks_to_register.reserve(chunk_cache_.size());

			glm::vec2 camera_pos_2d(camera.x, camera.z);

			for (auto const& [key, terrain_chunk] : chunk_cache_) {
				if (!render_manager_->HasChunk(key)) {
					// Calculate distance from chunk center to camera
					glm::vec2 chunk_center(
						key.first * scaled_chunk_size + scaled_chunk_size * 0.5f,
						key.second * scaled_chunk_size + scaled_chunk_size * 0.5f
					);
					float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);
					bool  in_frustum = isChunkInFrustum(frustum, key.first, key.second, scaled_chunk_size, max_h);

					// Register if in frustum OR if close enough to preload
					if (in_frustum || dist_sq < preload_distance_sq) {
						chunks_to_register.push_back({key, terrain_chunk, dist_sq, in_frustum});
					}
				}
			}

			// Sort by: in_frustum first, then by distance within each group
			std::sort(
				chunks_to_register.begin(),
				chunks_to_register.end(),
				[](const ChunkToRegister& a, const ChunkToRegister& b) {
					if (a.in_frustum != b.in_frustum) {
						return a.in_frustum; // In-frustum chunks first
					}
					return a.distance_sq < b.distance_sq;
				}
			);

			// Register closest chunks first
			int registrations_this_frame = 0;
			for (const auto& chunk : chunks_to_register) {
				if (registrations_this_frame >= max_registrations_per_frame)
					break;

				render_manager_->RegisterChunk(
					chunk.key,
					chunk.terrain->vertices,
					chunk.terrain->normals,
					chunk.terrain->GetIndices(),
					chunk.terrain->proxy.minY,
					chunk.terrain->proxy.maxY,
					glm::vec3(chunk.key.first * scaled_chunk_size, 0, chunk.key.second * scaled_chunk_size),
					chunk.terrain->occluders
				);
				registrations_this_frame++;
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

			// Collect visible chunks with distance info for sorting
			struct VisibleChunkInfo {
				std::shared_ptr<Terrain> terrain;
				float                    distance_sq;
			};

			std::vector<VisibleChunkInfo> visible_with_distance;
			visible_with_distance.reserve(chunk_cache_.size());

			glm::vec2 camera_pos_2d(camera.x, camera.z);

			for (auto const& [key, val] : chunk_cache_) {
				if (val && isChunkInFrustum(frustum, key.first, key.second, scaled_chunk_size, max_h)) {
					glm::vec2 chunk_center(
						key.first * scaled_chunk_size + scaled_chunk_size * 0.5f,
						key.second * scaled_chunk_size + scaled_chunk_size * 0.5f
					);
					float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);
					visible_with_distance.push_back({val, dist_sq});
				}
			}

			// Sort by distance (front-to-back)
			std::sort(
				visible_with_distance.begin(),
				visible_with_distance.end(),
				[](const VisibleChunkInfo& a, const VisibleChunkInfo& b) { return a.distance_sq < b.distance_sq; }
			);

			for (const auto& vci : visible_with_distance) {
				visible_chunks_.push_back(vci.terrain);
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

	void TerrainGenerator::SetWorldScale(float scale) {
		scale = std::max(0.0001f, scale); // Guard against division by zero

		if (std::abs(scale - world_scale_) < 0.0001f)
			return;

		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);

		world_scale_ = scale;
		terrain_version_++;

		// Cancel all pending tasks
		for (auto& pair : pending_chunks_) {
			pair.second.cancel();
		}
		pending_chunks_.clear();

		// Unregister from render manager and clear cache
		if (render_manager_) {
			for (auto const& [key, terrain] : chunk_cache_) {
				render_manager_->UnregisterChunk(key);
			}
		}
		chunk_cache_.clear();

		// Also clear visible chunks to prevent rendering stale data
		{
			std::lock_guard<std::mutex> visible_lock(visible_chunks_mutex_);
			visible_chunks_.clear();
		}
	}

	const std::vector<std::shared_ptr<Terrain>>& TerrainGenerator::GetVisibleChunks() const {
		std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
		return visible_chunks_;
	}

	std::vector<std::shared_ptr<Terrain>> TerrainGenerator::GetVisibleChunksCopy() const {
		std::lock_guard<std::mutex> lock(visible_chunks_mutex_);
		return visible_chunks_;
	}

	namespace {
		struct Rect {
			int start_row, start_col, num_rows, num_cols;
		};

		Rect findLargestRectangle(const std::vector<std::vector<bool>>& grid) {
			int rows = grid.size();
			if (rows == 0)
				return {0, 0, 0, 0};
			int cols = grid[0].size();

			std::vector<int> heights(cols, 0);
			int              maxArea = 0;
			Rect             bestRect = {0, 0, 0, 0};

			for (int i = 0; i < rows; ++i) {
				for (int j = 0; j < cols; ++j) {
					if (grid[i][j])
						heights[j]++;
					else
						heights[j] = 0;
				}

				// Largest rectangle in histogram for current row
				std::vector<int> stack;
				for (int j = 0; j <= cols; ++j) {
					int h = (j == cols) ? 0 : heights[j];
					while (!stack.empty() && h < heights[stack.back()]) {
						int hh = heights[stack.back()];
						stack.pop_back();
						int w = stack.empty() ? j : j - stack.back() - 1;
						int area = hh * w;
						if (area > maxArea) {
							maxArea = area;
							bestRect = {i - hh + 1, stack.empty() ? 0 : stack.back() + 1, hh, w};
						}
					}
					stack.push_back(j);
				}
			}
			return bestRect;
		}
	} // namespace

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
			float normalScale = floorScale; // * 0.4f;
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
		float sx = x / world_scale_;
		float sz = z / world_scale_;

		glm::vec3 path_data = getPathInfluence(sx, sz);
		float     path_factor = path_data.x;

		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));
		float     warp_strength = (1.0f - path_factor) * Constants::Class::Terrain::WarpStrength() * world_scale_;
		glm::vec2 warp = push_dir * warp_strength;

		glm::vec2 pos = glm::vec2(sx, sz);
		glm::vec2 warped_pos = pos + warp;

		float control_value = getBiomeControlValue(sx, sz);

		if (std::isnan(control_value) || std::isinf(control_value)) {
			control_value = 0.0f;
		}

		BiomeAttributes current;
		ApplyWeightedBiome(control_value, current);

		glm::vec3 terrain_height = biomefbm(warped_pos, current);

		float path_floor_level = -0.10f;
		terrain_height.x = glm::mix(path_floor_level, terrain_height.x, path_factor);

		// Scale height proportionally to world scale
		terrain_height.x *= world_scale_;
		// Derivatives stay the same to maintain slopes (h'(x) = f'(x/s) * 1/s * s = f'(x/s))

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

		// Check if this chunk has any deformations
		float scaled_chunk_size = static_cast<float>(chunk_size_) * world_scale_;
		float chunk_min_x = static_cast<float>(chunkX) * scaled_chunk_size;
		float chunk_min_z = static_cast<float>(chunkZ) * scaled_chunk_size;
		float chunk_max_x = chunk_min_x + scaled_chunk_size;
		float chunk_max_z = chunk_min_z + scaled_chunk_size;
		bool  chunk_has_deformations = deformation_manager_
										  .ChunkHasDeformations(chunk_min_x, chunk_min_z, chunk_max_x, chunk_max_z);

		// Generate heightmap

		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float worldX = (chunkX * chunk_size_ + i) * world_scale_;
				float worldZ = (chunkZ * chunk_size_ + j) * world_scale_;
				auto  noise = pointGenerate(worldX, worldZ);
				heightmap[i][j] = noise;
				has_terrain = has_terrain || noise[0] > 0;
			}
		}

		// Apply deformations if any affect this chunk
		if (chunk_has_deformations) {
			has_terrain = true; // Deformations can create terrain where there was none
			for (int i = 0; i < num_vertices_x; ++i) {
				for (int j = 0; j < num_vertices_z; ++j) {
					float worldX = (chunkX * chunk_size_ + i) * world_scale_;
					float worldZ = (chunkZ * chunk_size_ + j) * world_scale_;

					if (deformation_manager_.HasDeformationAt(worldX, worldZ)) {
						float     base_height = heightmap[i][j][0];
						glm::vec3 base_normal = diffToNorm(heightmap[i][j][1], heightmap[i][j][2]);

						auto result = deformation_manager_.QueryDeformations(worldX, worldZ, base_height, base_normal);

						if (result.has_deformation) {
							// Apply height delta
							heightmap[i][j][0] += result.total_height_delta;

							// Recompute gradient approximation for the deformed surface
							// We store the transformed normal info for later use
							// The gradient values are approximations - we'll use finite differences
							// after all heights are computed
						}
					}
				}
			}

			// Recompute normals using finite differences on deformed heightmap
			for (int i = 0; i < num_vertices_x; ++i) {
				for (int j = 0; j < num_vertices_z; ++j) {
					float worldX = (chunkX * chunk_size_ + i) * world_scale_;
					float worldZ = (chunkZ * chunk_size_ + j) * world_scale_;

					if (deformation_manager_.HasDeformationAt(worldX, worldZ)) {
						// Finite differences for gradient
						float h_center = heightmap[i][j][0];
						float h_left = (i > 0) ? heightmap[i - 1][j][0] : h_center;
						float h_right = (i < num_vertices_x - 1) ? heightmap[i + 1][j][0] : h_center;
						float h_down = (j > 0) ? heightmap[i][j - 1][0] : h_center;
						float h_up = (j < num_vertices_z - 1) ? heightmap[i][j + 1][0] : h_center;

						float dx = (h_right - h_left) * 0.5f;
						float dz = (h_up - h_down) * 0.5f;

						heightmap[i][j][1] = dx;
						heightmap[i][j][2] = dz;
					}
				}
			}
		}

		if (!has_terrain) {
			return {{}, {}, {}, {}, {}, chunkX, chunkZ, false};
		}

		// Generate vertices and normals
		positions.reserve(num_vertices_x * num_vertices_z);
		normals.reserve(num_vertices_x * num_vertices_z);
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				float y = heightmap[i][j][0];
				positions.emplace_back(i * world_scale_, y, j * world_scale_);
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

		// Prepare data for occluder generation
		std::vector<std::vector<float>> hmap_only(num_vertices_x, std::vector<float>(num_vertices_z));
		for (int i = 0; i < num_vertices_x; ++i) {
			for (int j = 0; j < num_vertices_z; ++j) {
				hmap_only[i][j] = heightmap[i][j][0];
			}
		}

		std::vector<OccluderQuad> occluders = _GenerateOccluders(hmap_only);

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

		return {indices, positions, normals, occluders, proxy, chunkX, chunkZ, true};
	}

	std::vector<OccluderQuad>
	TerrainGenerator::_GenerateOccluders(const std::vector<std::vector<float>>& hmap) const {
		std::vector<OccluderQuad> occluders;
		int                       rows = hmap.size();
		int                       cols = hmap[0].size();

		// Find min/max height in chunk
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				minY = std::min(minY, hmap[i][j]);
				maxY = std::max(maxY, hmap[i][j]);
			}
		}

		// Horizontal occluders at multiple levels
		const int numLevels = 4;
		for (int l = 0; l < numLevels; ++l) {
			float threshold = minY + (maxY - minY) * (float(l) / float(numLevels));

			std::vector<std::vector<bool>> grid(rows, std::vector<bool>(cols));
			for (int i = 0; i < rows; ++i) {
				for (int j = 0; j < cols; ++j) {
					grid[i][j] = hmap[i][j] >= threshold;
				}
			}

			// Find up to 2 quads per level
			for (int q = 0; q < 2; ++q) {
				Rect r = findLargestRectangle(grid);
				if (r.num_rows * r.num_cols < 16)
					break; // Too small

				OccluderQuad quad;
				// Corners in local chunk space (using indices as coordinates)
				// We subtract a small amount from height to stay safely inside
				float quadY = threshold - 0.1f * world_scale_;
				quad.corners[0] = glm::vec3(r.start_row * world_scale_, quadY, r.start_col * world_scale_);
				quad.corners[1] =
					glm::vec3((r.start_row + r.num_rows - 1) * world_scale_, quadY, r.start_col * world_scale_);
				quad.corners[2] = glm::vec3(
					(r.start_row + r.num_rows - 1) * world_scale_,
					quadY,
					(r.start_col + r.num_cols - 1) * world_scale_
				);
				quad.corners[3] =
					glm::vec3(r.start_row * world_scale_, quadY, (r.start_col + r.num_cols - 1) * world_scale_);
				occluders.push_back(quad);

				// Mark used area as false to find another rectangle
				for (int i = r.start_row; i < r.start_row + r.num_rows; ++i) {
					for (int j = r.start_col; j < r.start_col + r.num_cols; ++j) {
						grid[i][j] = false;
					}
				}
			}
		}

		// Vertical occluders (scanning along X - constant Z)
		for (int z = 0; z < cols; z += 8) {
			std::vector<std::vector<bool>> grid(rows, std::vector<bool>(20)); // Y levels
			float                          yStep = (maxY - minY) / 20.0f;
			if (yStep < 0.1f * world_scale_)
				continue;

			for (int i = 0; i < rows; ++i) {
				for (int yIdx = 0; yIdx < 20; ++yIdx) {
					float y = minY + yIdx * yStep;
					grid[i][yIdx] = hmap[i][z] >= y;
				}
			}

			Rect r = findLargestRectangle(grid);
			if (r.num_rows * r.num_cols > 20) {
				OccluderQuad quad;
				float        y0 = minY + r.start_col * yStep;
				float        y1 = minY + (r.start_col + r.num_cols - 1) * yStep;
				quad.corners[0] = glm::vec3(r.start_row * world_scale_, y0, z * world_scale_);
				quad.corners[1] = glm::vec3((r.start_row + r.num_rows - 1) * world_scale_, y0, z * world_scale_);
				quad.corners[2] = glm::vec3((r.start_row + r.num_rows - 1) * world_scale_, y1, z * world_scale_);
				quad.corners[3] = glm::vec3(r.start_row * world_scale_, y1, z * world_scale_);
				occluders.push_back(quad);
			}
		}

		// Vertical occluders (scanning along Z - constant X)
		for (int i = 0; i < rows; i += 8) {
			std::vector<std::vector<bool>> grid(cols, std::vector<bool>(20)); // Y levels
			float                          yStep = (maxY - minY) / 20.0f;
			if (yStep < 0.1f * world_scale_)
				continue;

			for (int z = 0; z < cols; ++z) {
				for (int yIdx = 0; yIdx < 20; ++yIdx) {
					float y = minY + yIdx * yStep;
					grid[z][yIdx] = hmap[i][z] >= y;
				}
			}

			Rect r = findLargestRectangle(grid);
			if (r.num_rows * r.num_cols > 20) {
				OccluderQuad quad;
				float        y0 = minY + r.start_col * yStep;
				float        y1 = minY + (r.start_col + r.num_cols - 1) * yStep;
				quad.corners[0] = glm::vec3(i * world_scale_, y0, r.start_row * world_scale_);
				quad.corners[1] = glm::vec3(i * world_scale_, y0, (r.start_row + r.num_rows - 1) * world_scale_);
				quad.corners[2] = glm::vec3(i * world_scale_, y1, (r.start_row + r.num_rows - 1) * world_scale_);
				quad.corners[3] = glm::vec3(i * world_scale_, y1, r.start_row * world_scale_);
				occluders.push_back(quad);
			}
		}

		return occluders;
	}

	bool
	TerrainGenerator::Raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, float& out_dist) const {
		float     step_size = 1.0f * world_scale_; // Initial step for ray marching
		float     current_dist = 0.0f;
		glm::vec3 current_pos = origin;

		// Ray marching to find a segment that contains the intersection
		while (current_dist < max_dist) {
			current_pos = origin + dir * current_dist;
			float terrain_height = std::get<0>(GetPointProperties(current_pos.x, current_pos.z));

			if (current_pos.y < terrain_height) {
				// We found an intersection between the previous and current step.
				// Now refine with a binary search.
				float start_dist = std::max(0.0f, current_dist - step_size);
				float end_dist = current_dist;

				constexpr int binary_search_steps = 10; // 10 steps for good precision
				for (int i = 0; i < binary_search_steps; ++i) {
					float     mid_dist = (start_dist + end_dist) / 2.0f;
					glm::vec3 mid_pos = origin + dir * mid_dist;

					float mid_terrain_height = std::get<0>(GetPointProperties(mid_pos.x, mid_pos.z));

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
			glm::vec3 path_data = Simplex::dnoise((sample_pos / world_scale_) * kPathFrequency);
			glm::vec2 gradient = glm::vec2(path_data.y, path_data.z);
			sample_pos -= (gradient * world_scale_) * path_data.x * kStepSize;
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
			glm::vec3 path_data = Simplex::dnoise((current_pos / world_scale_) * kPathFrequency);
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
				float worldX = (super_chunk_x * texture_dim + x) * world_scale_;
				float worldZ = (super_chunk_z * texture_dim + y) * world_scale_;

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
		// Note: coordinates already scaled if called from pointGenerate
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
		float     sx = x / world_scale_;
		float     sz = z / world_scale_;
		glm::vec3 path_data = getPathInfluence(sx, sz);
		float     path_factor = path_data.x;
		glm::vec2 push_dir = glm::normalize(glm::vec2(path_data.y, path_data.z));
		float     warp_strength = (1.0f - path_factor) * Constants::Class::Terrain::WarpStrength() * world_scale_;
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
		const int   kSuperChunkSizeInChunks = chunk_size_;
		const float scaled_texture_dim = kSuperChunkSizeInChunks * chunk_size_ * world_scale_;

		// Determine the range of superchunks needed
		int start_chunk_x = floor(static_cast<float>(world_x) / scaled_texture_dim);
		int end_chunk_x = floor(static_cast<float>(world_x + size - 1) / scaled_texture_dim);
		int start_chunk_z = floor(static_cast<float>(world_z) / scaled_texture_dim);
		int end_chunk_z = floor(static_cast<float>(world_z + size - 1) / scaled_texture_dim);

		// Create the destination texture
		std::vector<uint16_t> stitched_texture(size * size * 4);

		// Iterate over the needed superchunks and stitch them together
		for (int cz = start_chunk_z; cz <= end_chunk_z; ++cz) {
			for (int cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
				std::vector<uint16_t> superchunk = GenerateSuperChunkTexture(
					cx * static_cast<int>(scaled_texture_dim),
					cz * static_cast<int>(scaled_texture_dim)
				);

				// Calculate the region to copy from the superchunk
				int texture_dim = kSuperChunkSizeInChunks * chunk_size_;
				int src_start_x = std::max(0, static_cast<int>((world_x - cx * scaled_texture_dim) / world_scale_));
				int src_end_x = std::min(
					texture_dim,
					static_cast<int>((world_x + size - cx * scaled_texture_dim) / world_scale_)
				);
				int src_start_z = std::max(0, static_cast<int>((world_z - cz * scaled_texture_dim) / world_scale_));
				int src_end_z = std::min(
					texture_dim,
					static_cast<int>((world_z + size - cz * scaled_texture_dim) / world_scale_)
				);

				// Calculate the region to copy to in the destination texture
				int dest_start_x = std::max(0, static_cast<int>((cx * scaled_texture_dim - world_x) / world_scale_));
				int dest_start_z = std::max(0, static_cast<int>((cz * scaled_texture_dim - world_z) / world_scale_));

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

	// ========== Cache-Preferring Terrain Query Implementations ==========

	std::optional<std::tuple<float, glm::vec3>> TerrainGenerator::InterpolateFromCachedChunk(float x, float z) const {
		float scaled_chunk_size = static_cast<float>(chunk_size_) * world_scale_;

		// Determine which chunk this position belongs to
		int chunk_x = static_cast<int>(std::floor(x / scaled_chunk_size));
		int chunk_z = static_cast<int>(std::floor(z / scaled_chunk_size));

		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);

		auto it = chunk_cache_.find({chunk_x, chunk_z});
		if (it == chunk_cache_.end() || !it->second) {
			return std::nullopt; // Chunk not cached
		}

		const auto& terrain = it->second;
		const auto& vertices = terrain->vertices;
		const auto& normals = terrain->normals;

		if (vertices.empty()) {
			return std::nullopt;
		}

		// Convert world position to local chunk coordinates
		float chunk_origin_x = static_cast<float>(chunk_x) * scaled_chunk_size;
		float chunk_origin_z = static_cast<float>(chunk_z) * scaled_chunk_size;
		float local_x = x - chunk_origin_x;
		float local_z = z - chunk_origin_z;

		// Terrain mesh is generated with world_scale_ spacing between vertices
		// The chunk has (chunk_size_ + 1) vertices along each edge
		int grid_size = chunk_size_ + 1;

		// Find the grid cell in vertex units [0, chunk_size]
		int ix = static_cast<int>(std::floor(local_x / world_scale_));
		int iz = static_cast<int>(std::floor(local_z / world_scale_));

		// Clamp to valid range
		ix = std::clamp(ix, 0, chunk_size_ - 1);
		iz = std::clamp(iz, 0, chunk_size_ - 1);

		// Get the 4 corner vertex indices
		int idx00 = iz * grid_size + ix;
		int idx10 = iz * grid_size + (ix + 1);
		int idx01 = (iz + 1) * grid_size + ix;
		int idx11 = (iz + 1) * grid_size + (ix + 1);

		// Bounds check
		if (idx11 >= static_cast<int>(vertices.size())) {
			return std::nullopt;
		}

		// Bilinear interpolation factors
		float fx = (local_x / world_scale_) - static_cast<float>(ix);
		float fz = (local_z / world_scale_) - static_cast<float>(iz);
		fx = std::clamp(fx, 0.0f, 1.0f);
		fz = std::clamp(fz, 0.0f, 1.0f);

		// Interpolate position (we really just need height, Y component)
		glm::vec3 v00 = vertices[idx00];
		glm::vec3 v10 = vertices[idx10];
		glm::vec3 v01 = vertices[idx01];
		glm::vec3 v11 = vertices[idx11];

		float h00 = v00.y;
		float h10 = v10.y;
		float h01 = v01.y;
		float h11 = v11.y;

		float height = h00 * (1 - fx) * (1 - fz) + h10 * fx * (1 - fz) + h01 * (1 - fx) * fz + h11 * fx * fz;

		// Interpolate normal
		glm::vec3 n00 = normals[idx00];
		glm::vec3 n10 = normals[idx10];
		glm::vec3 n01 = normals[idx01];
		glm::vec3 n11 = normals[idx11];

		glm::vec3 normal = n00 * (1 - fx) * (1 - fz) + n10 * fx * (1 - fz) + n01 * (1 - fx) * fz + n11 * fx * fz;
		normal = glm::normalize(normal);

		return std::make_tuple(height, normal);
	}

	bool TerrainGenerator::IsPositionCached(float x, float z) const {
		float scaled_chunk_size = static_cast<float>(chunk_size_) * world_scale_;
		int   chunk_x = static_cast<int>(std::floor(x / scaled_chunk_size));
		int   chunk_z = static_cast<int>(std::floor(z / scaled_chunk_size));

		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);
		return chunk_cache_.find({chunk_x, chunk_z}) != chunk_cache_.end();
	}

	std::tuple<float, glm::vec3> TerrainGenerator::GetCachedPointProperties(float x, float z) const {
		// Try cache first
		auto cached = InterpolateFromCachedChunk(x, z);
		if (cached.has_value()) {
			return cached.value();
		}

		// Fall back to procedural generation
		return pointProperties(x, z);
	}

	bool TerrainGenerator::IsPointBelowTerrain(const glm::vec3& point) const {
		auto [height, normal] = GetCachedPointProperties(point.x, point.z);
		return point.y < height;
	}

	float TerrainGenerator::GetDistanceAboveTerrain(const glm::vec3& point) const {
		auto [height, normal] = GetCachedPointProperties(point.x, point.z);
		return point.y - height;
	}

	std::tuple<float, glm::vec3> TerrainGenerator::GetClosestTerrainInfo(const glm::vec3& point) const {
		auto [height, normal] = GetCachedPointProperties(point.x, point.z);

		// The closest point on terrain directly below/above the query point
		glm::vec3 terrain_point(point.x, height, point.z);
		glm::vec3 to_terrain = terrain_point - point;
		float     distance = glm::length(to_terrain);

		if (distance < 0.0001f) {
			// Point is essentially on the terrain surface
			return {0.0f, normal};
		}

		// Direction to the closest terrain point
		glm::vec3 direction = to_terrain / distance;

		// If point is below terrain, we want to point upward (toward the surface)
		// The normal already points "up" from the terrain surface
		if (point.y < height) {
			// Below terrain - use the surface normal as the escape direction
			direction = normal;
			distance = height - point.y; // Vertical distance to surface
		}

		return {distance, direction};
	}

	bool TerrainGenerator::RaycastCached(
		const glm::vec3& origin,
		const glm::vec3& direction,
		float            max_distance,
		float&           out_distance,
		glm::vec3&       out_normal
	) const {
		// Use smaller step size for more precision, adaptive based on terrain scale
		float     step_size = 0.5f * world_scale_;
		float     current_dist = 0.0f;
		glm::vec3 dir = glm::normalize(direction);

		// Track if we're using cached data or not
		bool all_cached = true;

		// Ray marching with cache-preferring height queries
		glm::vec3 prev_pos = origin;
		float     prev_height = 0.0f;
		bool      prev_valid = false;

		while (current_dist < max_distance) {
			glm::vec3 current_pos = origin + dir * current_dist;

			// Try to get height from cache
			auto cached = InterpolateFromCachedChunk(current_pos.x, current_pos.z);

			float     terrain_height;
			glm::vec3 surface_normal;

			if (cached.has_value()) {
				std::tie(terrain_height, surface_normal) = cached.value();
			} else {
				// Fall back to procedural - note we're no longer fully cached
				all_cached = false;
				std::tie(terrain_height, surface_normal) = pointProperties(current_pos.x, current_pos.z);
			}

			if (current_pos.y < terrain_height) {
				// We found an intersection - refine with binary search
				float start_dist = prev_valid ? (current_dist - step_size) : 0.0f;
				float end_dist = current_dist;

				constexpr int binary_search_steps = 8;
				for (int i = 0; i < binary_search_steps; ++i) {
					float     mid_dist = (start_dist + end_dist) * 0.5f;
					glm::vec3 mid_pos = origin + dir * mid_dist;

					auto  mid_cached = InterpolateFromCachedChunk(mid_pos.x, mid_pos.z);
					float mid_height;
					if (mid_cached.has_value()) {
						mid_height = std::get<0>(mid_cached.value());
					} else {
						mid_height = std::get<0>(pointProperties(mid_pos.x, mid_pos.z));
					}

					if (mid_pos.y < mid_height) {
						end_dist = mid_dist;
					} else {
						start_dist = mid_dist;
					}
				}

				out_distance = (start_dist + end_dist) * 0.5f;

				// Get the normal at the hit point
				glm::vec3 hit_pos = origin + dir * out_distance;
				auto      hit_cached = InterpolateFromCachedChunk(hit_pos.x, hit_pos.z);
				if (hit_cached.has_value()) {
					out_normal = std::get<1>(hit_cached.value());
				} else {
					out_normal = std::get<1>(pointProperties(hit_pos.x, hit_pos.z));
				}

				return true;
			}

			prev_pos = current_pos;
			prev_height = terrain_height;
			prev_valid = true;
			current_dist += step_size;
		}

		return false; // No hit
	}

	// ==================== Terrain Deformation Convenience Methods ====================

	uint32_t TerrainGenerator::AddCrater(
		const glm::vec3& center,
		float            radius,
		float            depth,
		float            irregularity,
		float            rim_height
	) {
		static std::atomic<uint32_t> crater_id_counter{1};
		uint32_t                     id = crater_id_counter++;

		// Scale crater parameters by world scale so it maintains relative size
		float s_radius = radius * world_scale_;
		float s_depth = depth * world_scale_;
		float s_rim_height = rim_height * world_scale_;

		auto crater = std::make_shared<CraterDeformation>(
			id,
			center,
			s_radius,
			s_depth,
			irregularity,
			s_rim_height,
			static_cast<uint32_t>(eng_())
		);

		deformation_manager_.AddDeformation(crater);
		InvalidateDeformedChunks(id);

		return id;
	}

	uint32_t TerrainGenerator::AddFlattenSquare(
		const glm::vec3& center,
		float            half_width,
		float            half_depth,
		float            blend_distance,
		float            rotation_y
	) {
		static std::atomic<uint32_t> flatten_id_counter{1000000}; // Offset to avoid collision with craters
		uint32_t                     id = flatten_id_counter++;

		// Scale parameters by world scale
		float s_half_width = half_width * world_scale_;
		float s_half_depth = half_depth * world_scale_;
		float s_blend_distance = blend_distance * world_scale_;

		auto flatten = std::make_shared<FlattenSquareDeformation>(
			id,
			center,
			s_half_width,
			s_half_depth,
			s_blend_distance,
			rotation_y
		);

		deformation_manager_.AddDeformation(flatten);
		InvalidateDeformedChunks(id);

		return id;
	}

	void TerrainGenerator::InvalidateDeformedChunks(std::optional<uint32_t> deformation_id) {
		std::lock_guard<std::recursive_mutex> lock(chunk_cache_mutex_);
		terrain_version_++;

		std::vector<std::pair<int, int>> chunks_to_regenerate;
		float                            scaled_chunk_size = static_cast<float>(chunk_size_) * world_scale_;

		if (deformation_id.has_value()) {
			// Invalidate only chunks affected by the specific deformation
			auto deformation = deformation_manager_.GetDeformation(deformation_id.value());
			if (!deformation) {
				return;
			}

			glm::vec3 def_min, def_max;
			deformation->GetBounds(def_min, def_max);

			// Convert to chunk coordinates
			int min_chunk_x = static_cast<int>(std::floor(def_min.x / scaled_chunk_size));
			int max_chunk_x = static_cast<int>(std::floor(def_max.x / scaled_chunk_size));
			int min_chunk_z = static_cast<int>(std::floor(def_min.z / scaled_chunk_size));
			int max_chunk_z = static_cast<int>(std::floor(def_max.z / scaled_chunk_size));

			for (auto& [chunk_key, terrain] : chunk_cache_) {
				if (chunk_key.first >= min_chunk_x && chunk_key.first <= max_chunk_x &&
				    chunk_key.second >= min_chunk_z && chunk_key.second <= max_chunk_z) {
					chunks_to_regenerate.push_back(chunk_key);
				}
			}
		} else {
			// Invalidate all chunks that have any deformation
			for (auto& [chunk_key, terrain] : chunk_cache_) {
				float chunk_min_x = static_cast<float>(chunk_key.first) * scaled_chunk_size;
				float chunk_min_z = static_cast<float>(chunk_key.second) * scaled_chunk_size;
				float chunk_max_x = chunk_min_x + scaled_chunk_size;
				float chunk_max_z = chunk_min_z + scaled_chunk_size;

				if (deformation_manager_.ChunkHasDeformations(chunk_min_x, chunk_min_z, chunk_max_x, chunk_max_z)) {
					chunks_to_regenerate.push_back(chunk_key);
				}
			}
		}

		// Cancel any pending chunks that would be affected (they'll have stale data)
		for (const auto& chunk_key : chunks_to_regenerate) {
			auto pending_it = pending_chunks_.find(chunk_key);
			if (pending_it != pending_chunks_.end()) {
				pending_it->second.cancel();
				pending_chunks_.erase(pending_it);
			}
		}

		// Regenerate chunks in-place to avoid visual holes
		// Strategy: Generate new data first, then atomically swap
		for (const auto& chunk_key : chunks_to_regenerate) {
			// Generate the updated chunk data (includes deformations)
			TerrainGenerationResult result = generateChunkData(chunk_key.first, chunk_key.second);

			if (result.has_terrain) {
				// Create new terrain object with deformed data
				auto new_terrain = std::make_shared<Terrain>(
					result.indices,
					result.positions,
					result.normals,
					result.proxy,
					result.occluders
				);
				new_terrain->SetPosition(result.chunk_x * scaled_chunk_size, 0, result.chunk_z * scaled_chunk_size);

				if (render_manager_) {
					new_terrain->SetManagedByRenderManager(true);
					// RegisterChunk handles updates - if chunk already exists, it just updates the heightmap texture
					// This is the key: the texture update happens in-place without removing the chunk first
					render_manager_->RegisterChunk(
						chunk_key,
						new_terrain->vertices,
						new_terrain->normals,
						new_terrain->GetIndices(),
						new_terrain->proxy.minY,
						new_terrain->proxy.maxY,
						glm::vec3(chunk_key.first * scaled_chunk_size, 0, chunk_key.second * scaled_chunk_size),
						new_terrain->occluders
					);
				} else {
					// Legacy rendering: set up mesh (will replace old GPU resources)
					new_terrain->setupMesh();
				}

				// Atomically replace the cache entry
				// The old Terrain object will be destroyed, freeing its resources
				chunk_cache_[chunk_key] = new_terrain;
			} else {
				// Deformation removed all terrain from this chunk (rare case)
				// In this case we do need to remove it
				if (render_manager_) {
					render_manager_->UnregisterChunk(chunk_key);
				}
				chunk_cache_.erase(chunk_key);
			}
		}
	}

} // namespace Boidsish
