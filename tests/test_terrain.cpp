#include <gtest/gtest.h>
#include "terrain_generator.h"
#include "graphics.h"

using namespace Boidsish;

TEST(ChunkKeyTest, ImplicitConversionAndOrdering) {
    ChunkKey default_key;
    EXPECT_EQ(default_key.lod, 0);
    EXPECT_EQ(default_key.x, 0);
    EXPECT_EQ(default_key.z, 0);

    ChunkKey pair_key(std::make_pair(5, -3));
    EXPECT_EQ(pair_key.lod, 0);
    EXPECT_EQ(pair_key.x, 5);
    EXPECT_EQ(pair_key.z, -3);

    std::pair<int, int> pair_conv = pair_key;
    EXPECT_EQ(pair_conv.first, 5);
    EXPECT_EQ(pair_conv.second, -3);

    ChunkKey k0(0, 1, 2);
    ChunkKey k1(1, 1, 2);
    ChunkKey k0_dup(0, 1, 2);

    EXPECT_TRUE(k0 < k1);
    EXPECT_FALSE(k1 < k0);
    EXPECT_EQ(k0, k0_dup);
    EXPECT_NE(k0, k1);
}

TEST(TerrainLodRingsTest, MultipleRingsGenerationAndSameTextureSize) {
    TerrainGenerator gen(12345);
    gen.SetNumLodLevels(3);
    gen.SetLodScaleMultiplier(2.0f);

    EXPECT_EQ(gen.GetNumLodLevels(), 3);
    EXPECT_FLOAT_EQ(gen.GetLodScaleMultiplier(), 2.0f);

    Frustum frustum;
    for (int i = 0; i < 6; ++i) {
        frustum.planes[i].normal = glm::vec3(0, 1, 0);
        frustum.planes[i].distance = 1e5f; // All planes far away so everything is in frustum
    }

    Camera camera;
    camera.x = 0.0f;
    camera.y = 10.0f;
    camera.z = 0.0f;

    gen.WaitForAllChunks(frustum, camera);

    const auto& visible = gen.GetVisibleChunks();
    EXPECT_FALSE(visible.empty());

    int res = gen.GetChunkSize() + 1; // 33
    size_t expected_height_normal_size = res * res * 4;

    for (const auto& terrain : visible) {
        EXPECT_EQ(terrain->packed_height_normal.size(), expected_height_normal_size);
        EXPECT_EQ(terrain->packed_biomes.size(), expected_height_normal_size);
    }
}

TEST(TerrainLodRingsTest, CachedPropertyQueryAcrossLods) {
    TerrainGenerator gen(42);
    gen.SetNumLodLevels(4);
    gen.SetLodScaleMultiplier(2.0f);

    Frustum frustum;
    for (int i = 0; i < 6; ++i) {
        frustum.planes[i].normal = glm::vec3(0, 1, 0);
        frustum.planes[i].distance = 1e5f;
    }

    Camera camera;
    camera.x = 0.0f;
    camera.y = 10.0f;
    camera.z = 0.0f;

    gen.WaitForAllChunks(frustum, camera);

    // Query close to camera (Level 0)
    EXPECT_TRUE(gen.IsPositionCached(0.0f, 0.0f));
    auto [h0, n0] = gen.GetTerrainPropertiesAtPoint(0.0f, 0.0f);
    EXPECT_TRUE(std::isfinite(h0));
    EXPECT_FLOAT_EQ(glm::length(n0), 1.0f);

    // Query further away (Level 1 / 2)
    float far_dist = static_cast<float>(gen.GetChunkSize()) * gen.GetWorldScale() * 4.0f;
    EXPECT_TRUE(gen.IsPositionCached(far_dist, far_dist));
    auto [h_far, n_far] = gen.GetTerrainPropertiesAtPoint(far_dist, far_dist);
    EXPECT_TRUE(std::isfinite(h_far));
    EXPECT_FLOAT_EQ(glm::length(n_far), 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
