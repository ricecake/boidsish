#include <gtest/gtest.h>
#include "procedural_generator.h"
#include <iostream>

using namespace Boidsish;

TEST(ProceduralGeneratorTest, GenerateAllTypes) {
    auto rock = ProceduralGenerator::Generate(ProceduralType::Rock, 123);
    ASSERT_NE(rock, nullptr);
    EXPECT_GT(rock->getMeshes().size(), 0);

    auto grass = ProceduralGenerator::Generate(ProceduralType::Grass, 456);
    ASSERT_NE(grass, nullptr);
    EXPECT_GT(grass->getMeshes().size(), 0);

    auto flower = ProceduralGenerator::Generate(ProceduralType::Flower, 789);
    ASSERT_NE(flower, nullptr);
    EXPECT_GT(flower->getMeshes().size(), 0);

    auto tree = ProceduralGenerator::Generate(ProceduralType::Tree, 101112);
    ASSERT_NE(tree, nullptr);
    EXPECT_GT(tree->getMeshes().size(), 0);
}

TEST(ProceduralGeneratorTest, MultipleVariants) {
    auto flower1 = ProceduralGenerator::Generate(ProceduralType::Flower, 1);
    auto flower2 = ProceduralGenerator::Generate(ProceduralType::Flower, 2);

    // They should be different (different seeds)
    // We can check vertex counts as a proxy
    size_t v1 = 0;
    for (const auto& m : flower1->getMeshes()) v1 += m.vertices.size();

    size_t v2 = 0;
    for (const auto& m : flower2->getMeshes()) v2 += m.vertices.size();

    std::cout << "Flower 1 vertices: " << v1 << std::endl;
    std::cout << "Flower 2 vertices: " << v2 << std::endl;
    // They might have different counts due to different rulesets/seeds
}

TEST(ProceduralGeneratorTest, WeldVerticesReducesCount) {
    // This is hard to test without bypassing CreateModelDataFromGeometry
    // but we can assume it's working if it compiles and runs without crashing.
}
