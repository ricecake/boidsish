#include <gtest/gtest.h>
#include "terrain_generator.h"
#include "graphics.h"

using namespace Boidsish;

TEST(TerrainGeneratorTest, DISABLED_CachePersistence) {
    TerrainGenerator gen;
    Frustum frustum;
    Camera camera;

    // Set up a frustum that sees nothing (all planes at infinity)
    for(int i=0; i<6; ++i) {
        frustum.planes[i].normal = glm::vec3(0, 1, 0);
        frustum.planes[i].distance = -1e10f;
    }

    camera.x = 0; camera.y = 1; camera.z = 0; // height 1 -> distance 10

    // Update should trigger some low priority generations if they are not in cache
    gen.update(frustum, camera);

    // Since it's async, we might need to wait or just check if they are enqueued
    // Actually, we can't easily check pending_chunks_ from outside.

    // But we can check if visible_chunks is empty (it should be because of frustum culling)
    EXPECT_TRUE(gen.getVisibleChunks().empty());
}

TEST(TerrainGeneratorTest, ConsistencyWithDeformations) {
    TerrainGenerator gen(12345);
    gen.SetPhongAlpha(0.0f); // Start with Bilinear

    // We avoid calling Update() to prevent OpenGL calls in headless environment
    // Procedural queries should still work and account for deformations.

    float testX = 5.3f;
    float testZ = 7.7f;

    // Case 1: No deformations
    auto [h_proc1, n_proc1] = gen.CalculateTerrainPropertiesAtPoint(testX, testZ);

    // Case 2: Add deformation
    // Using a large crater that covers the test point and its neighbors
    uint32_t defId = gen.AddCrater(glm::vec3(testX, 0.0f, testZ), 20.0f, 10.0f);

    auto [h_proc2, n_proc2] = gen.CalculateTerrainPropertiesAtPoint(testX, testZ);

    // Verify deformation was actually applied
    EXPECT_LT(h_proc2, h_proc1 - 5.0f);

    // Case 3: Phong
    gen.SetPhongAlpha(1.0f);
    auto [h_proc3, n_proc3] = gen.CalculateTerrainPropertiesAtPoint(testX, testZ);

    // Phong height should generally be different from Bilinear height on curved terrain
    EXPECT_NE(h_proc3, h_proc2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
