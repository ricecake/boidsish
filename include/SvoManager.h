#pragma once

#include <vector>
#include <memory>
#include <queue>
#include <cstdint>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"

namespace Boidsish {

	class ServiceLocator;

	// A sparse voxel representation on CPU
	struct SvoVoxel {
		glm::ivec3 position; // coordinate in [0, 2^depth - 1]^3
		glm::vec4 color;     // color / material
	};

	// GPU-friendly layout for SVO Node
	struct SvoNodeGPU {
		uint32_t child_mask = 0;       // Bit i is 1 if child i exists
		uint32_t leaf_mask = 0;        // Bit i is 1 if child i is a leaf
		uint32_t child_base_idx = 0;   // Base index of internal child nodes in u_svoNodes
		uint32_t voxel_base_idx = 0;   // Base index of leaf children in u_svoVoxels
	};

	// Internal building representation
	struct SvoBuildNode {
		uint32_t child_mask = 0;
		uint32_t leaf_mask = 0;
		uint32_t children[8] = {0};
		uint32_t voxel_idx[8] = {0};
	};

	class SvoManager : public IManager {
	public:
		explicit SvoManager(ServiceLocator& loc);
		~SvoManager() override;

		void Initialize() override;
		void Shutdown() override;

		/**
		 * @brief Builds the Sparse Voxel Octree from a collection of sparse voxels.
		 * The builder assumes coordinates are within the range [0, 2^max_depth - 1]^3.
		 * @param voxels Input collection of sparse voxels.
		 * @param max_depth The maximum depth/resolution of the octree.
		 */
		void BuildAndUpload(const std::vector<SvoVoxel>& voxels, uint32_t max_depth);

		/**
		 * @brief Binds both SVO Nodes and SVO Voxels SSBOs to their respective binding points.
		 */
		void BindSSBOs() const;

		GLuint GetNodesSSBO() const { return nodes_ssbo_; }
		GLuint GetVoxelsSSBO() const { return voxels_ssbo_; }

		uint32_t GetDepth() const { return max_depth_; }
		uint32_t GetNodeCount() const { return node_count_; }
		uint32_t GetVoxelCount() const { return voxel_count_; }

		uint32_t BuildSvoNodeRecursive(
			const std::vector<SvoVoxel>& voxels,
			const std::vector<size_t>& voxel_indices,
			glm::ivec3 min_bound,
			glm::ivec3 max_bound,
			uint32_t current_depth,
			uint32_t max_depth,
			std::vector<SvoBuildNode>& build_nodes,
			std::vector<glm::vec4>& build_voxels
		);

		std::vector<SvoNodeGPU> FlattenSvo(
			const std::vector<SvoBuildNode>& build_nodes,
			const std::vector<glm::vec4>& build_voxels,
			std::vector<glm::vec4>& out_flat_voxels
		);

	private:
		GLuint nodes_ssbo_ = 0;
		GLuint voxels_ssbo_ = 0;
		bool initialized_ = false;

		uint32_t max_depth_ = 0;
		uint32_t node_count_ = 0;
		uint32_t voxel_count_ = 0;
	};

} // namespace Boidsish
