#include "structure_terrain_generator.h"

#include <cmath>
#include <algorithm>
#include "frustum.h"
#include "graphics.h"
#include "logger.h"

namespace Boidsish {

	StructureTerrainGenerator::StructureTerrainGenerator(int seed) : seed_(seed), thread_pool_() {}

	StructureTerrainGenerator::~StructureTerrainGenerator() {
		for (auto& pair : pending_chunks_) {
			pair.second.cancel();
		}
		pending_chunks_.clear();
		chunk_cache_.clear();
	}

	void StructureTerrainGenerator::Update(const Frustum& frustum, const Camera& camera) {
		float scaled_chunk_size = static_cast<float>(GetChunkSize()) * world_scale_;
		int current_chunk_x = static_cast<int>(std::floor(camera.x / scaled_chunk_size));
		int current_chunk_z = static_cast<int>(std::floor(camera.z / scaled_chunk_size));

		std::lock_guard<std::mutex> lock(mutex_);

		// 1. Load/Generate
		for (int x = current_chunk_x - view_distance_; x <= current_chunk_x + view_distance_; ++x) {
			for (int z = current_chunk_z - view_distance_; z <= current_chunk_z + view_distance_; ++z) {
				std::pair<int, int> key = {x, z};
				if (chunk_cache_.find(key) == chunk_cache_.end() && pending_chunks_.find(key) == pending_chunks_.end()) {
					pending_chunks_.emplace(key, thread_pool_.enqueue(TaskPriority::MEDIUM, &StructureTerrainGenerator::_GenerateChunkData, this, x, z));
				}
			}
		}

		// 2. Collect completed
		std::vector<std::pair<int, int>> completed;
		for (auto& pair : pending_chunks_) {
			if (pair.second.is_ready()) {
				auto result = pair.second.get();
				auto terrain_chunk = std::make_shared<Terrain>(
					result.indices,
					result.positions,
					result.normals,
					result.biomes,
					result.proxy
				);
				terrain_chunk->SetPosition(result.chunk_x * scaled_chunk_size, 0, result.chunk_z * scaled_chunk_size);

				if (render_manager_) {
					terrain_chunk->SetManagedByRenderManager(true);
					render_manager_->RegisterChunk(
						pair.first,
						result.positions,
						result.normals,
						result.biomes,
						result.indices,
						result.proxy.minY,
						result.proxy.maxY,
						glm::vec3(result.chunk_x * scaled_chunk_size, 0, result.chunk_z * scaled_chunk_size)
					);
				}

				chunk_cache_[pair.first] = terrain_chunk;
				completed.push_back(pair.first);
			}
		}
		for (auto& key : completed) pending_chunks_.erase(key);

		// 3. Visibility and LRU
		visible_chunks_.clear();
		std::vector<std::pair<int, int>> to_remove;
		for (auto& pair : chunk_cache_) {
			int dx = std::abs(pair.first.first - current_chunk_x);
			int dz = std::abs(pair.first.second - current_chunk_z);
			if (dx > view_distance_ + 2 || dz > view_distance_ + 2) {
				to_remove.push_back(pair.first);
				continue;
			}
			visible_chunks_.push_back(pair.second);
		}
		for (auto& key : to_remove) {
			if (render_manager_) render_manager_->UnregisterChunk(key);
			chunk_cache_.erase(key);
		}
	}

	StructureTerrainGenerator::StructureGenerationResult StructureTerrainGenerator::_GenerateChunkData(int chunkX, int chunkZ) {
		StructureGenerationResult res;
		res.chunk_x = chunkX;
		res.chunk_z = chunkZ;

		int grid_res = 8;
		float scaled_chunk_size = static_cast<float>(GetChunkSize()) * world_scale_;
		float step = scaled_chunk_size / grid_res;

		float floor_y = 0.0f;
		float ceil_y = 10.0f * world_scale_;

		// Generate Floor (upward normal)
		for (int x = 0; x <= grid_res; ++x) {
			for (int z = 0; z <= grid_res; ++z) {
				res.positions.push_back(glm::vec3(x * step, floor_y, z * step));
				res.normals.push_back(glm::vec3(0, 1, 0));
				res.biomes.push_back(glm::vec2(1, 0)); // LushGrass
			}
		}

		// Generate Ceiling (downward normal)
		int ceil_offset = res.positions.size();
		for (int x = 0; x <= grid_res; ++x) {
			for (int z = 0; z <= grid_res; ++z) {
				res.positions.push_back(glm::vec3(x * step, ceil_y, z * step));
				res.normals.push_back(glm::vec3(0, -1, 0));
				res.biomes.push_back(glm::vec2(6, 0)); // GreyRock
			}
		}

		// Generate Walls
		int wall1_offset = res.positions.size();
		for (int x = 0; x <= grid_res; ++x) {
			for (int y = 0; y <= 1; ++y) {
				float curr_y = (y == 0) ? floor_y : ceil_y;
				res.positions.push_back(glm::vec3(x * step, curr_y, 0.0f));
				res.normals.push_back(glm::vec3(0, 0, 1));
				res.biomes.push_back(glm::vec2(6, 0));
			}
		}

		int wall2_offset = res.positions.size();
		for (int x = 0; x <= grid_res; ++x) {
			for (int y = 0; y <= 1; ++y) {
				float curr_y = (y == 0) ? floor_y : ceil_y;
				res.positions.push_back(glm::vec3(x * step, curr_y, scaled_chunk_size));
				res.normals.push_back(glm::vec3(0, 0, -1));
				res.biomes.push_back(glm::vec2(6, 0));
			}
		}

		// Wall 3 (at x=0)
		int wall3_offset = res.positions.size();
		for (int z = 0; z <= grid_res; ++z) {
			for (int y = 0; y <= 1; ++y) {
				float curr_y = (y == 0) ? floor_y : ceil_y;
				res.positions.push_back(glm::vec3(0.0f, curr_y, z * step));
				res.normals.push_back(glm::vec3(1, 0, 0));
				res.biomes.push_back(glm::vec2(6, 0));
			}
		}

		// Wall 4 (at x=scaled_chunk_size)
		int wall4_offset = res.positions.size();
		for (int z = 0; z <= grid_res; ++z) {
			for (int y = 0; y <= 1; ++y) {
				float curr_y = (y == 0) ? floor_y : ceil_y;
				res.positions.push_back(glm::vec3(scaled_chunk_size, curr_y, z * step));
				res.normals.push_back(glm::vec3(-1, 0, 0));
				res.biomes.push_back(glm::vec2(6, 0));
			}
		}

		// Indices for floor (CCW from above)
		for (int x = 0; x < grid_res; ++x) {
			for (int z = 0; z < grid_res; ++z) {
				int i0 = x * (grid_res + 1) + z;
				int i1 = (x + 1) * (grid_res + 1) + z;
				int i2 = (x + 1) * (grid_res + 1) + (z + 1);
				int i3 = x * (grid_res + 1) + (z + 1);

				res.indices.push_back(i0);
				res.indices.push_back(i1);
				res.indices.push_back(i2);

				res.indices.push_back(i0);
				res.indices.push_back(i2);
				res.indices.push_back(i3);
			}
		}

		// Indices for walls
		for (int i = 0; i < grid_res; ++i) {
			// Wall 1 (at z=0, looking inward = CCW)
			int w1_bl = wall1_offset + i * 2;
			int w1_tl = wall1_offset + i * 2 + 1;
			int w1_br = wall1_offset + (i + 1) * 2;
			int w1_tr = wall1_offset + (i + 1) * 2 + 1;
			res.indices.push_back(w1_bl);
			res.indices.push_back(w1_br);
			res.indices.push_back(w1_tr);
			res.indices.push_back(w1_bl);
			res.indices.push_back(w1_tr);
			res.indices.push_back(w1_tl);

			// Wall 2 (at z=scaled_chunk_size, looking inward = CCW)
			int w2_bl = wall2_offset + i * 2;
			int w2_tl = wall2_offset + i * 2 + 1;
			int w2_br = wall2_offset + (i + 1) * 2;
			int w2_tr = wall2_offset + (i + 1) * 2 + 1;
			res.indices.push_back(w2_bl);
			res.indices.push_back(w2_tl);
			res.indices.push_back(w2_tr);
			res.indices.push_back(w2_bl);
			res.indices.push_back(w2_tr);
			res.indices.push_back(w2_br);

			// Wall 3 (at x=0, looking inward = CCW)
			int w3_bl = wall3_offset + i * 2;
			int w3_tl = wall3_offset + i * 2 + 1;
			int w3_br = wall3_offset + (i + 1) * 2;
			int w3_tr = wall3_offset + (i + 1) * 2 + 1;
			res.indices.push_back(w3_bl);
			res.indices.push_back(w3_tl);
			res.indices.push_back(w3_tr);
			res.indices.push_back(w3_bl);
			res.indices.push_back(w3_tr);
			res.indices.push_back(w3_br);

			// Wall 4 (at x=scaled_chunk_size, looking inward = CCW)
			int w4_bl = wall4_offset + i * 2;
			int w4_tl = wall4_offset + i * 2 + 1;
			int w4_br = wall4_offset + (i + 1) * 2;
			int w4_tr = wall4_offset + (i + 1) * 2 + 1;
			res.indices.push_back(w4_bl);
			res.indices.push_back(w4_br);
			res.indices.push_back(w4_tr);
			res.indices.push_back(w4_bl);
			res.indices.push_back(w4_tr);
			res.indices.push_back(w4_tl);
		}

		// Indices for ceiling (CCW looking down from below)
		for (int x = 0; x < grid_res; ++x) {
			for (int z = 0; z < grid_res; ++z) {
				int i0 = ceil_offset + x * (grid_res + 1) + z;
				int i1 = ceil_offset + (x + 1) * (grid_res + 1) + z;
				int i2 = ceil_offset + (x + 1) * (grid_res + 1) + (z + 1);
				int i3 = ceil_offset + x * (grid_res + 1) + (z + 1);

				res.indices.push_back(i0);
				res.indices.push_back(i2);
				res.indices.push_back(i1);

				res.indices.push_back(i0);
				res.indices.push_back(i3);
				res.indices.push_back(i2);
			}
		}

		res.proxy.minY = floor_y;
		res.proxy.maxY = ceil_y;
		res.proxy.center = glm::vec3(scaled_chunk_size * 0.5f, (floor_y + ceil_y) * 0.5f, scaled_chunk_size * 0.5f);

		return res;
	}

	const std::vector<std::shared_ptr<Terrain>>& StructureTerrainGenerator::GetVisibleChunks() const {
		return visible_chunks_;
	}

	std::vector<std::shared_ptr<Terrain>> StructureTerrainGenerator::GetVisibleChunksCopy() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return visible_chunks_;
	}

	std::tuple<float, glm::vec3> StructureTerrainGenerator::CalculateTerrainPropertiesAtPoint(float x, float z) const {
		// Enclosed structure: floor is at 0
		return {0.0f, glm::vec3(0, 1, 0)};
	}

	std::tuple<float, glm::vec3> StructureTerrainGenerator::GetTerrainPropertiesAtPoint(float x, float z) const {
		return CalculateTerrainPropertiesAtPoint(x, z);
	}

	bool StructureTerrainGenerator::IsPointBelowTerrain(const glm::vec3& point) const {
		// Inside the structure if between 0 and 10*world_scale
		return point.y < 0.0f || point.y > 10.0f * world_scale_;
	}

	float StructureTerrainGenerator::GetDistanceAboveTerrain(const glm::vec3& point) const {
		return point.y;
	}

	bool StructureTerrainGenerator::IsPositionCached(float x, float z) const {
		return true;
	}

	bool StructureTerrainGenerator::Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float& out_distance) const {
		glm::vec3 dummy_normal;
		return RaycastCached(origin, direction, max_distance, out_distance, dummy_normal);
	}

	bool StructureTerrainGenerator::RaycastCached(
		const glm::vec3& origin,
		const glm::vec3& direction,
		float            max_distance,
		float&           out_distance,
		glm::vec3&       out_normal
	) const {
		float floor_y = 0.0f;
		float ceil_y = 10.0f * world_scale_;

		// If ray starts outside, check if it hits the boundary
		if (origin.y <= floor_y || origin.y >= ceil_y) {
			// simplified: just check floor/ceil intersection
			if (direction.y > 0.0001f && origin.y <= floor_y) {
				float t = (floor_y - origin.y) / direction.y;
				if (t >= 0 && t <= max_distance) {
					out_distance = t;
					out_normal = glm::vec3(0, 1, 0);
					return true;
				}
			} else if (direction.y < -0.0001f && origin.y >= ceil_y) {
				float t = (ceil_y - origin.y) / direction.y;
				if (t >= 0 && t <= max_distance) {
					out_distance = t;
					out_normal = glm::vec3(0, -1, 0);
					return true;
				}
			}
			return false;
		}

		// Ray starts inside. It MUST hit a boundary if long enough.
		float t_floor = -1.0f, t_ceil = -1.0f;
		if (direction.y < -0.0001f) t_floor = (floor_y - origin.y) / direction.y;
		if (direction.y > 0.0001f)  t_ceil = (ceil_y - origin.y) / direction.y;

		if (t_floor >= 0 && (t_ceil < 0 || t_floor < t_ceil)) {
			if (t_floor <= max_distance) {
				out_distance = t_floor;
				out_normal = glm::vec3(0, 1, 0);
				return true;
			}
		} else if (t_ceil >= 0) {
			if (t_ceil <= max_distance) {
				out_distance = t_ceil;
				out_normal = glm::vec3(0, -1, 0);
				return true;
			}
		}

		return false;
	}

} // namespace Boidsish
