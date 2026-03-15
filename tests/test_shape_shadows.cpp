#include <gtest/gtest.h>
#include "dot.h"
#include <glm/glm.hpp>
#include <optional>

using namespace Boidsish;

class GenericShape : public Shape {
public:
    void render() const override {}
    void render(Shader&, const glm::mat4&) const override {}
    glm::mat4 GetModelMatrix() const override { return glm::mat4(1.0f); }
    std::string GetInstanceKey() const override { return "GenericShape"; }
};

TEST(ShapeShadowTest, DefaultCastsShadows) {
    GenericShape shape;
    // By default, Shape should cast shadows if not colossal
    EXPECT_TRUE(shape.CastsShadows());

    shape.SetColossal(true);
    EXPECT_FALSE(shape.CastsShadows());
}

TEST(ShapeShadowTest, DotDefaultNoShadows) {
    Dot dot(1, 0.0f, 0.0f, 0.0f, 1.0f);
    // Dot overrides GetDefaultCastsShadows to return false
    EXPECT_FALSE(dot.CastsShadows());
}

TEST(ShapeShadowTest, ShadowOverride) {
    GenericShape shape;
    EXPECT_TRUE(shape.CastsShadows());

    // Override to false
    shape.SetShadowOverride(false);
    EXPECT_FALSE(shape.CastsShadows());

    // Clear override
    shape.SetShadowOverride(std::nullopt);
    EXPECT_TRUE(shape.CastsShadows());

    Dot dot(1, 0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FALSE(dot.CastsShadows());

    // Override Dot to true
    dot.SetShadowOverride(true);
    EXPECT_TRUE(dot.CastsShadows());

    // Clear override
    dot.SetShadowOverride(std::nullopt);
    EXPECT_FALSE(dot.CastsShadows());
}
