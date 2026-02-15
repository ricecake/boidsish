#include <gtest/gtest.h>
#include "terrain_deformation_manager.h"
#include "terrain_deformations.h"
#include <iostream>

using namespace Boidsish;

TEST(TerrainDeformationManagerTest, OverlappingDeformations) {
    TerrainDeformationManager manager(0.5);

    glm::vec3 center(0.0f, 0.0f, 0.0f);
    float radius = 10.0f;
    float depth = 5.0f;

    // Add first crater
    auto crater1 = std::make_shared<CraterDeformation>(1, center, radius, depth, 0.0f, 0.0f, 0);
    manager.AddDeformation(crater1);

    // Add second crater at the same location
    auto crater2 = std::make_shared<CraterDeformation>(2, center, radius, depth, 0.0f, 0.0f, 0);
    manager.AddDeformation(crater2);

    // Query at center
    auto result = manager.QueryDeformations(0.0f, 0.0f, 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    std::cout << "Total height delta: " << result.total_height_delta << std::endl;
    std::cout << "Affecting deformations: " << result.affecting_deformations.size() << std::endl;

    // Each crater should contribute -depth
    // If summed, it should be -10.0. If overwritten, it will be -5.0.
    EXPECT_NEAR(result.total_height_delta, -10.0f, 0.001f);
    EXPECT_EQ(result.affecting_deformations.size(), 2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
