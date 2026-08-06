#include "SvoManager.h"
#include "constants.h"
#include "logger.h"
#include "service_locator.h"
#include <algorithm>
#include <iostream>

namespace Boidsish {

	SvoManager::SvoManager(ServiceLocator& /*loc*/) {}

	SvoManager::~SvoManager() {
		Shutdown();
	}

	void SvoManager::Initialize() {
		if (initialized_)
			return;

		glGenBuffers(1, &nodes_ssbo_);
		glGenBuffers(1, &voxels_ssbo_);

		initialized_ = true;
		logger::INFO("SvoManager successfully initialized SVO SSBOs");
	}

	void SvoManager::Shutdown() {
		if (nodes_ssbo_ != 0) {
			glDeleteBuffers(1, &nodes_ssbo_);
			nodes_ssbo_ = 0;
		}
		if (voxels_ssbo_ != 0) {
			glDeleteBuffers(1, &voxels_ssbo_);
			voxels_ssbo_ = 0;
		}
		initialized_ = false;
	}

	void SvoManager::BuildAndUpload(const std::vector<SvoVoxel>& voxels, uint32_t max_depth) {
		max_depth_ = max_depth;
		if (voxels.empty() || max_depth == 0) {
			node_count_ = 0;
			voxel_count_ = 0;
			return;
		}

		// Collect all indices of the voxels
		std::vector<size_t> voxel_indices(voxels.size());
		for (size_t i = 0; i < voxels.size(); ++i) {
			voxel_indices[i] = i;
		}

		std::vector<SvoBuildNode> build_nodes;
		std::vector<glm::vec4> build_voxels;

		glm::ivec3 min_bound(0);
		glm::ivec3 max_bound((1 << max_depth) - 1);

		// 1. Build the CPU-side SVO recursively
		BuildSvoNodeRecursive(
			voxels,
			voxel_indices,
			min_bound,
			max_bound,
			0, // current_depth
			max_depth,
			build_nodes,
			build_voxels
		);

		// 2. Flatten the SVO nodes and voxel data level-by-level (breadth-first)
		std::vector<glm::vec4> flat_voxels;
		std::vector<SvoNodeGPU> flat_nodes = FlattenSvo(build_nodes, build_voxels, flat_voxels);

		node_count_ = static_cast<uint32_t>(flat_nodes.size());
		voxel_count_ = static_cast<uint32_t>(flat_voxels.size());

		// 3. Upload SVO nodes and voxels to the GPU
		if (initialized_) {
			if (!flat_nodes.empty()) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodes_ssbo_);
				glBufferData(GL_SHADER_STORAGE_BUFFER, flat_nodes.size() * sizeof(SvoNodeGPU), flat_nodes.data(), GL_STATIC_DRAW);
			}
			if (!flat_voxels.empty()) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxels_ssbo_);
				glBufferData(GL_SHADER_STORAGE_BUFFER, flat_voxels.size() * sizeof(glm::vec4), flat_voxels.data(), GL_STATIC_DRAW);
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			logger::INFO("SvoManager: SVO uploaded successfully. Nodes: " + std::to_string(node_count_) +
				", Voxels: " + std::to_string(voxel_count_));
		}
	}

	uint32_t SvoManager::BuildSvoNodeRecursive(
		const std::vector<SvoVoxel>& voxels,
		const std::vector<size_t>& voxel_indices,
		glm::ivec3 min_bound,
		glm::ivec3 max_bound,
		uint32_t current_depth,
		uint32_t max_depth,
		std::vector<SvoBuildNode>& build_nodes,
		std::vector<glm::vec4>& build_voxels
	) {
		if (voxel_indices.empty()) {
			return 0;
		}

		uint32_t node_idx = static_cast<uint32_t>(build_nodes.size());
		build_nodes.emplace_back();

		glm::ivec3 center = min_bound + (max_bound - min_bound) / 2;

		// Group voxel indices into 8 octants
		std::vector<size_t> octant_voxels[8];
		for (size_t idx : voxel_indices) {
			const auto& vox = voxels[idx];
			uint32_t octant = 0;
			if (vox.position.x > center.x) octant |= 1;
			if (vox.position.y > center.y) octant |= 2;
			if (vox.position.z > center.z) octant |= 4;
			octant_voxels[octant].push_back(idx);
		}

		uint32_t child_mask = 0;
		uint32_t leaf_mask = 0;

		for (uint32_t octant = 0; octant < 8; ++octant) {
			if (octant_voxels[octant].empty()) continue;

			child_mask |= (1u << octant);

			glm::ivec3 oct_min = min_bound;
			glm::ivec3 oct_max = max_bound;

			if (octant & 1) oct_min.x = center.x + 1; else oct_max.x = center.x;
			if (octant & 2) oct_min.y = center.y + 1; else oct_max.y = center.y;
			if (octant & 4) oct_min.z = center.z + 1; else oct_max.z = center.z;

			if (current_depth == max_depth - 1) {
				leaf_mask |= (1u << octant);
				// Standard color averaging for the leaf region
				glm::vec4 avg_color(0.0f);
				for (size_t idx : octant_voxels[octant]) {
					avg_color += voxels[idx].color;
				}
				avg_color /= static_cast<float>(octant_voxels[octant].size());

				uint32_t vox_idx = static_cast<uint32_t>(build_voxels.size());
				build_voxels.push_back(avg_color);
				build_nodes[node_idx].voxel_idx[octant] = vox_idx;
			} else {
				uint32_t child_node_idx = BuildSvoNodeRecursive(
					voxels,
					octant_voxels[octant],
					oct_min,
					oct_max,
					current_depth + 1,
					max_depth,
					build_nodes,
					build_voxels
				);
				build_nodes[node_idx].children[octant] = child_node_idx;
			}
		}

		build_nodes[node_idx].child_mask = child_mask;
		build_nodes[node_idx].leaf_mask = leaf_mask;

		return node_idx;
	}

	std::vector<SvoNodeGPU> SvoManager::FlattenSvo(
		const std::vector<SvoBuildNode>& build_nodes,
		const std::vector<glm::vec4>& build_voxels,
		std::vector<glm::vec4>& out_flat_voxels
	) {
		if (build_nodes.empty()) return {};

		std::vector<SvoNodeGPU> flat_nodes;
		flat_nodes.reserve(build_nodes.size());

		struct QueueEntry {
			uint32_t build_idx;
			uint32_t flat_idx;
		};
		std::queue<QueueEntry> q;

		flat_nodes.emplace_back();
		q.push({0, 0});

		while (!q.empty()) {
			auto entry = q.front();
			q.pop();

			const auto& bnode = build_nodes[entry.build_idx];
			flat_nodes[entry.flat_idx].child_mask = bnode.child_mask;
			flat_nodes[entry.flat_idx].leaf_mask = bnode.leaf_mask;

			uint32_t internal_child_count = 0;
			uint32_t leaf_child_count = 0;

			for (uint32_t octant = 0; octant < 8; ++octant) {
				if (bnode.child_mask & (1u << octant)) {
					if (bnode.leaf_mask & (1u << octant)) {
						leaf_child_count++;
					} else {
						internal_child_count++;
					}
				}
			}

			// Allocate all internal children consecutively
			if (internal_child_count > 0) {
				uint32_t child_base = static_cast<uint32_t>(flat_nodes.size());
				flat_nodes[entry.flat_idx].child_base_idx = child_base;

				for (uint32_t i = 0; i < internal_child_count; ++i) {
					flat_nodes.emplace_back();
				}

				uint32_t child_idx_offset = 0;
				for (uint32_t octant = 0; octant < 8; ++octant) {
					if (bnode.child_mask & (1u << octant)) {
						if (!(bnode.leaf_mask & (1u << octant))) {
							uint32_t child_flat_idx = child_base + child_idx_offset;
							child_idx_offset++;
							q.push({bnode.children[octant], child_flat_idx});
						}
					}
				}
			}

			// Allocate all leaf voxel colors consecutively
			if (leaf_child_count > 0) {
				uint32_t voxel_base = static_cast<uint32_t>(out_flat_voxels.size());
				flat_nodes[entry.flat_idx].voxel_base_idx = voxel_base;

				for (uint32_t octant = 0; octant < 8; ++octant) {
					if (bnode.child_mask & (1u << octant)) {
						if (bnode.leaf_mask & (1u << octant)) {
							uint32_t build_vox_idx = bnode.voxel_idx[octant];
							out_flat_voxels.push_back(build_voxels[build_vox_idx]);
						}
					}
				}
			}
		}

		return flat_nodes;
	}

	void SvoManager::BindSSBOs() const {
		if (initialized_) {
			if (nodes_ssbo_ != 0) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::SvoNodes(), nodes_ssbo_);
			}
			if (voxels_ssbo_ != 0) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::SvoVoxels(), voxels_ssbo_);
			}
		}
	}

} // namespace Boidsish
