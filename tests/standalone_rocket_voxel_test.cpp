#include "rocket_voxel_tree.h"
#include <iostream>
#include <cassert>
#include <glm/glm.hpp>

using namespace Boidsish;

int main() {
	RocketVoxelTree tree(1.0);

	std::cout << "Adding segment..." << std::endl;
	tree.AddSegment(glm::vec3(0, 0, 0), glm::vec3(2, 0, 0), 10.0f);

	auto voxels = tree.GetActiveVoxels();
	std::cout << "Active voxels: " << voxels.size() << std::endl;
	for (const auto& v : voxels) {
		std::cout << "  Voxel at (" << v.position.x << ", " << v.position.y << ", " << v.position.z
				  << ") time: " << v.timestamp << std::endl;
	}
	assert(voxels.size() >= 3);

	std::cout << "Pruning (active)..." << std::endl;
	tree.Prune(11.0f, 5.0f); // 11 - 10 = 1 < 5, should stay
	assert(tree.GetActiveCount() >= 3);

	std::cout << "Testing bounded retrieval..." << std::endl;
	auto bounded = tree.GetActiveVoxels(glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, 0.5f, 0.5f));
	std::cout << "Bounded voxels (center only): " << bounded.size() << std::endl;
	assert(bounded.size() == 1);

	std::cout << "Pruning (expired)..." << std::endl;
	tree.Prune(20.0f, 5.0f); // 20 - 10 = 10 > 5, should be removed
	assert(tree.GetActiveCount() == 0);

	std::cout << "Test passed!" << std::endl;
	return 0;
}
