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
		float chunk_size = static_cast<float>(GetChunkSize()) * world_scale_;
		int current_chunk_x = static_cast<int>(std::floor(camera.x / chunk_size));
		int current_chunk_z = static_cast<int>(std::floor(camera.z / chunk_size));

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
				terrain_chunk->SetPosition(result.chunk_x * chunk_size, 0, result.chunk_z * chunk_size);

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
						glm::vec3(result.chunk_x * chunk_size, 0, result.chunk_z * chunk_size)
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

		int grid_res = 8; // Low res for example
		float step = static_cast<float>(GetChunkSize()) / grid_res;

		float floor_y = 0.0f;
		float ceil_y = 10.0f;

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

		// Generate Walls (Optional: only on chunk boundaries for "corridor" feel, but let's do simple side walls)
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
				res.positions.push_back(glm::vec3(x * step, curr_y, GetChunkSize()));
				res.normals.push_back(glm::vec3(0, 0, -1));
				res.biomes.push_back(glm::vec2(6, 0));
			}
		}

		// Indices for floor (CCW)
		for (int x = 0; x < grid_res; ++x) {
			for (int z = 0; z < grid_res; ++z) {
				int r0 = x * (grid_res + 1) + z;
				int r1 = (x + 1) * (grid_res + 1) + z;

				res.indices.push_back(r0);
				res.indices.push_back(r0 + 1);
				res.indices.push_back(r1 + 1);

				res.indices.push_back(r0);
				res.indices.push_back(r1 + 1);
				res.indices.push_back(r1);
			}
		}

		// Indices for walls
		for (int x = 0; x < grid_res; ++x) {
			// Wall 1
			int w1_0 = wall1_offset + x * 2;
			int w1_1 = wall1_offset + (x + 1) * 2;
			res.indices.push_back(w1_0);
			res.indices.push_back(w1_1);
			res.indices.push_back(w1_0 + 1);
			res.indices.push_back(w1_1);
			res.indices.push_back(w1_1 + 1);
			res.indices.push_back(w1_0 + 1);

			// Wall 2
			int w2_0 = wall2_offset + x * 2;
			int w2_1 = wall2_offset + (x + 1) * 2;
			res.indices.push_back(w2_0);
			res.indices.push_back(w2_0 + 1);
			res.indices.push_back(w2_1);
			res.indices.push_back(w2_1);
			res.indices.push_back(w2_0 + 1);
			res.indices.push_back(w2_1 + 1);
		}

		// Indices for ceiling (CCW from below)
		for (int x = 0; x < grid_res; ++x) {
			for (int z = 0; z < grid_res; ++z) {
				int r0 = ceil_offset + x * (grid_res + 1) + z;
				int r1 = ceil_offset + (x + 1) * (grid_res + 1) + z;

				res.indices.push_back(r0);
				res.indices.push_back(r1 + 1);
				res.indices.push_back(r0 + 1);

				res.indices.push_back(r0);
				res.indices.push_back(r1);
				res.indices.push_back(r1 + 1);
			}
		}

		res.proxy.minY = floor_y;
		res.proxy.maxY = ceil_y;
		res.proxy.center = glm::vec3(GetChunkSize() * 0.5f, (floor_y + ceil_y) * 0.5f, GetChunkSize() * 0.5f);

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
		// Inside the structure if between 0 and 10
		return point.y < 0.0f || point.y > 10.0f;
	}

	float StructureTerrainGenerator::GetDistanceAboveTerrain(const glm::vec3& point) const {
		return point.y;
	}

	bool StructureTerrainGenerator::IsPositionCached(float x, float z) const {
		return true;
	}

	bool StructureTerrainGenerator::Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, float& out_distance) const {
		return false;
	}

	bool StructureTerrainGenerator::RaycastCached(
		const glm::vec3& origin,
		const glm::vec3& direction,
		float            max_distance,
		float&           out_distance,
		glm::vec3&       out_normal
	) const {
		return false;
	}

} // namespace Boidsish
