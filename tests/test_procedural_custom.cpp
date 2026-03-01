#include <gtest/gtest.h>
#include "procedural_generator.h"

using namespace Boidsish;

TEST(ProceduralGeneratorCustomTest, CustomFlowerGeneration) {
    std::string axiom = "F";
    std::vector<std::string> rules = {"F=FF"};
    // This should produce a very simple flower
    auto model = ProceduralGenerator::GenerateFlower(123, axiom, rules);
    ASSERT_NE(model, nullptr);
}

TEST(ProceduralGeneratorCustomTest, CustomTreeGeneration) {
    std::string axiom = "X";
    std::vector<std::string> rules = {"X=F", "F=FF"};
    auto model = ProceduralGenerator::GenerateTree(456, axiom, rules, 2);
    ASSERT_NE(model, nullptr);
}
