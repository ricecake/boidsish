#include <gtest/gtest.h>
#include "procedural_generator.h"

using namespace Boidsish;

TEST(ProceduralGeneratorCustomTest, CustomFlowerGeneration) {
    // Axiom and rules are empty to use default, but with iterations=1 for maximum speed/safety
    auto model = ProceduralGenerator::GenerateFlower(123, "", {}, 1);
    ASSERT_NE(model, nullptr);
}

// TEST(ProceduralGeneratorCustomTest, CustomTreeGeneration) {
//     // Default tree with iterations=1
//     auto model = ProceduralGenerator::GenerateTree(456, "", {}, 1);
//     ASSERT_NE(model, nullptr);
// }
