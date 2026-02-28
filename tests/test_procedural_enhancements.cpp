#include <gtest/gtest.h>
#include "procedural_generator.h"
#include <iostream>

using namespace Boidsish;

TEST(ProceduralGeneratorTest, GenerateCritter) {
    auto critter = ProceduralGenerator::Generate(ProceduralType::Critter, 1337);
    ASSERT_NE(critter, nullptr);
    EXPECT_GT(critter->getMeshes().size(), 0);

    size_t totalVertices = 0;
    for (const auto& mesh : critter->getMeshes()) {
        totalVertices += mesh.vertices.size();
        // Check if vertex colors are enabled (proxy: has_vertex_colors flag)
        EXPECT_TRUE(mesh.has_vertex_colors);
    }
    EXPECT_GT(totalVertices, 0);

    // Check AABB alignment (bottom should be at Y=0 or close to it due to float precision)
    auto aabb = critter->GetAABB();
    EXPECT_NEAR(aabb.min.y, 0.0f, 0.001f);

    // Check for bones
    auto data = critter->GetData();
    EXPECT_GT(data->bone_count, 0);
    EXPECT_FALSE(data->bone_info_map.empty());
}

TEST(ProceduralGeneratorTest, FlowerNewShapes) {
    // Axiom that uses new symbols: 'B' for button, 'L' for leaf, digits for variants
    std::string axiom = "FB'1L2L";
    auto flower = ProceduralGenerator::GenerateFlower(123, axiom, {}, 1);
    ASSERT_NE(flower, nullptr);
    EXPECT_GT(flower->getMeshes().size(), 0);

    bool foundVertexColor = false;
    for (const auto& mesh : flower->getMeshes()) {
        if (mesh.has_vertex_colors) foundVertexColor = true;
    }
    EXPECT_TRUE(foundVertexColor);
}
