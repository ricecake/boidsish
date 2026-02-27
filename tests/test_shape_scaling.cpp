#include <gtest/gtest.h>
#include "dot.h"
#include <glm/glm.hpp>

using namespace Boidsish;

TEST(ShapeScalingTest, SetScaleToMaxDimension) {
    Dot dot(1, 0.0f, 0.0f, 0.0f, 100.0f); // Size 100 -> Radius 1.0 -> Dim 2.0

    // Scale X to 10.0. Scaling factor should be 10.0 / 2.0 = 5.0
    dot.SetScaleToMaxDimension(10.0f, 0);

    EXPECT_FLOAT_EQ(dot.GetScale().x, 5.0f);
    EXPECT_FLOAT_EQ(dot.GetScale().y, 5.0f);
    EXPECT_FLOAT_EQ(dot.GetScale().z, 5.0f);

    AABB aabb = dot.GetAABB();
    EXPECT_FLOAT_EQ(aabb.max.x - aabb.min.x, 10.0f);
    EXPECT_FLOAT_EQ(aabb.max.y - aabb.min.y, 10.0f);
    EXPECT_FLOAT_EQ(aabb.max.z - aabb.min.z, 10.0f);
}

TEST(ShapeScalingTest, SetScaleRelativeTo) {
    Dot dotA(1, 0.0f, 0.0f, 0.0f, 100.0f); // Dim 2.0
    Dot dotB(2, 0.0f, 0.0f, 0.0f, 200.0f); // Dim 4.0

    // Set dotA to be half the length of dotB in Y
    dotA.SetScaleRelativeTo(dotB, 0.5f, 1);

    // dotB dim Y is 4.0. Half is 2.0.
    // dotA current dim Y is 2.0. So scaling factor is 1.0.
    EXPECT_FLOAT_EQ(dotA.GetScale().y, 1.0f);

    AABB aabbA = dotA.GetAABB();
    AABB aabbB = dotB.GetAABB();
    EXPECT_FLOAT_EQ(aabbA.max.y - aabbA.min.y, (aabbB.max.y - aabbB.min.y) * 0.5f);
}

TEST(ShapeScalingTest, SetScaleToFitInside) {
    Dot dotA(1, 0.0f, 0.0f, 0.0f, 100.0f);
    dotA.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    // dotA dimensions: 2x2x2

    Dot dotB(2, 0.0f, 0.0f, 0.0f, 100.0f);
    // Let's make dotB an AABB of 10x4x20 (world space)
    // We can't easily do this with Dot because it's a sphere.
    // But we can just use its AABB as a reference.
    // Wait, SetScaleToFitInside will use dotB.GetAABB().

    // If I want a non-cube AABB for dotB, I should use something else.
    // Or I can just Mock something, but I don't want to add too much complexity.
    // I can create a simple subclass of Shape for testing.
}

class TestShape : public Shape {
public:
    TestShape(const glm::vec3& min, const glm::vec3& max) : Shape() {
        local_aabb_ = AABB(min, max);
    }
    void render() const override {}
    void render(Shader&, const glm::mat4&) const override {}
    glm::mat4 GetModelMatrix() const override {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
        model *= glm::mat4_cast(GetRotation());
        model = glm::scale(model, GetScale());
        return model;
    }
    std::string GetInstanceKey() const override { return "TestShape"; }
};

TEST(ShapeScalingTest, SetScaleToFitInsideComplex) {
    TestShape small(glm::vec3(-1, -2, -0.5), glm::vec3(1, 2, 0.5));
    // Dimensions: X=2, Y=4, Z=1

    TestShape large(glm::vec3(-5, -5, -5), glm::vec3(5, 5, 5));
    // Dimensions: X=10, Y=10, Z=10

    // Fit small inside large.
    // X factor: 10/2 = 5
    // Y factor: 10/4 = 2.5
    // Z factor: 10/1 = 10
    // Min factor is 2.5

    small.SetScaleToFitInside(large);

    EXPECT_FLOAT_EQ(small.GetScale().x, 2.5f);
    EXPECT_FLOAT_EQ(small.GetScale().y, 2.5f);
    EXPECT_FLOAT_EQ(small.GetScale().z, 2.5f);

    AABB aabbSmall = small.GetAABB();
    AABB aabbLarge = large.GetAABB();

    EXPECT_LE(aabbSmall.max.x - aabbSmall.min.x, aabbLarge.max.x - aabbLarge.min.x + 0.001f);
    EXPECT_LE(aabbSmall.max.y - aabbSmall.min.y, aabbLarge.max.y - aabbLarge.min.y + 0.001f);
    EXPECT_LE(aabbSmall.max.z - aabbSmall.min.z, aabbLarge.max.z - aabbLarge.min.z + 0.001f);

    EXPECT_NEAR(aabbSmall.max.y - aabbSmall.min.y, aabbLarge.max.y - aabbLarge.min.y, 0.001f);
}
