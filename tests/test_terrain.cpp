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
    gen.Update(frustum, camera);

    // Since it's async, we might need to wait or just check if they are enqueued
    // Actually, we can't easily check pending_chunks_ from outside.

    // But we can check if visible_chunks is empty (it should be because of frustum culling)
    EXPECT_TRUE(gen.GetVisibleChunks().empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
