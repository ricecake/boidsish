#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <iostream>
#include "SvoManager.h"
#include "service_locator.h"

namespace Boidsish {

	class SvoTest : public ::testing::Test {
	protected:
		std::unique_ptr<ServiceLocator> locator;
		std::unique_ptr<SvoManager> manager;

		void SetUp() override {
			locator = std::make_unique<ServiceLocator>();
			manager = std::make_unique<SvoManager>(*locator);
		}
	};

	TEST_F(SvoTest, EmptySvo) {
		std::vector<SvoVoxel> voxels;
		manager->BuildAndUpload(voxels, 3);

		EXPECT_EQ(manager->GetNodeCount(), 0u);
		EXPECT_EQ(manager->GetVoxelCount(), 0u);
		EXPECT_EQ(manager->GetDepth(), 3u);
	}

	TEST_F(SvoTest, SingleVoxelSvo) {
		std::vector<SvoVoxel> voxels = {
			{ glm::ivec3(2, 2, 2), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }
		};
		manager->BuildAndUpload(voxels, 3); // Max bound is (1 << 3) - 1 = 7

		EXPECT_GT(manager->GetNodeCount(), 0u);
		EXPECT_EQ(manager->GetVoxelCount(), 1u);
	}

	TEST_F(SvoTest, CustomSparseSvo) {
		// Build SVO with three distinct voxels at different octants of the root:
		// Root is size 8x8x8. Center is at (3, 3, 3) because min=0, max=7, center = 0 + (7-0)/2 = 3.
		// Voxel 1: (1, 1, 1) -> octant 0 (x <= 3, y <= 3, z <= 3)
		// Voxel 2: (5, 1, 1) -> octant 1 (x > 3, y <= 3, z <= 3)
		// Voxel 3: (5, 5, 5) -> octant 7 (x > 3, y > 3, z > 3)
		std::vector<SvoVoxel> voxels = {
			{ glm::ivec3(1, 1, 1), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
			{ glm::ivec3(5, 1, 1), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
			{ glm::ivec3(5, 5, 5), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) }
		};

		manager->BuildAndUpload(voxels, 3);

		EXPECT_GT(manager->GetNodeCount(), 0u);
		EXPECT_EQ(manager->GetVoxelCount(), 3u);
	}

	TEST_F(SvoTest, FlattenAndBitwiseCorrectness) {
		// Create a carefully designed set of sparse voxels to verify mask indices
		std::vector<SvoVoxel> voxels = {
			{ glm::ivec3(1, 1, 1), glm::vec4(0.1f, 0.2f, 0.3f, 1.0f) }, // octant 0 at root, and sub-octants down to depth 3
			{ glm::ivec3(5, 1, 1), glm::vec4(0.5f, 0.6f, 0.7f, 1.0f) }  // octant 1 at root, and sub-octants down to depth 3
		};

		std::vector<SvoBuildNode> build_nodes;
		std::vector<glm::vec4> build_voxels;
		std::vector<size_t> voxel_indices = { 0, 1 };

		manager->BuildSvoNodeRecursive(
			voxels,
			voxel_indices,
			glm::ivec3(0),
			glm::ivec3(7),
			0, // current_depth
			3, // max_depth
			build_nodes,
			build_voxels
		);

		std::vector<glm::vec4> flat_voxels;
		std::vector<SvoNodeGPU> flat_nodes = manager->FlattenSvo(build_nodes, build_voxels, flat_voxels);

		ASSERT_FALSE(flat_nodes.empty());

		// Root node (index 0) must have child_mask matching octants 0 and 1
		EXPECT_EQ(flat_nodes[0].child_mask, (1u << 0) | (1u << 1));

		// Root node's children cannot be leaves yet because max_depth is 3 and root is at depth 0
		EXPECT_EQ(flat_nodes[0].leaf_mask, 0u);

		// The root has 2 internal child nodes (since none are leaves).
		// Root's child_base_idx should point to the start of its children
		uint32_t root_child_base = flat_nodes[0].child_base_idx;
		EXPECT_GT(root_child_base, 0u);

		// Verify GLSL-compatible bitCount indexing
		for (uint32_t octant = 0; octant < 8; ++octant) {
			if (flat_nodes[0].child_mask & (1u << octant)) {
				uint32_t leaf_flag = flat_nodes[0].leaf_mask & (1u << octant);
				EXPECT_EQ(leaf_flag, 0u); // Not a leaf at depth 0

				// Calculate offset using child_mask and leaf_mask
				uint32_t internal_mask = flat_nodes[0].child_mask & ~flat_nodes[0].leaf_mask;
				uint32_t active_bits_before = internal_mask & ((1u << octant) - 1u);
				uint32_t pop_count = 0;
				for (int b = 0; b < 32; ++b) {
					if (active_bits_before & (1u << b)) {
						pop_count++;
					}
				}

				uint32_t child_idx = root_child_base + pop_count;
				EXPECT_LT(child_idx, flat_nodes.size());
			}
		}
	}

} // namespace Boidsish
