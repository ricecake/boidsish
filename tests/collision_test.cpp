#include <gtest/gtest.h>
#include "collision.h"
#include "shape.h"
#include "dot.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

TEST(CollisionTest, RayAABBIntersection) {
    AABB aabb(glm::vec3(-1.0f), glm::vec3(1.0f));

    // Ray hitting front
    Ray ray1(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    float t;
    EXPECT_TRUE(aabb.Intersects(ray1, t));
    EXPECT_FLOAT_EQ(t, 4.0f);

    // Ray missing
    Ray ray2(glm::vec3(2.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_FALSE(aabb.Intersects(ray2, t));
}

TEST(CollisionTest, AABBTransform) {
    AABB aabb(glm::vec3(-1.0f), glm::vec3(1.0f));
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
    AABB transformed = aabb.Transform(transform);

    EXPECT_FLOAT_EQ(transformed.min.x, 9.0f);
    EXPECT_FLOAT_EQ(transformed.max.x, 11.0f);
    EXPECT_FLOAT_EQ(transformed.min.y, -1.0f);
    EXPECT_FLOAT_EQ(transformed.max.y, 1.0f);
}

TEST(CollisionTest, DotCollision) {
    // Dot size 100 -> combined radius 1.0
    Dot dot(1, 10.0f, 0.0f, 0.0f, 100.0f);

    Ray ray(glm::vec3(10.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    float t;
    EXPECT_TRUE(dot.Intersects(ray, t));
    EXPECT_FLOAT_EQ(t, 4.0f);

    // Ray missing dot
    Ray ray_miss(glm::vec3(12.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_FALSE(dot.Intersects(ray_miss, t));
}

TEST(CollisionTest, ShapeClampingProperties) {
    Dot dot(1);
    EXPECT_FALSE(dot.IsClampedToTerrain());
    dot.SetClampedToTerrain(true);
    EXPECT_TRUE(dot.IsClampedToTerrain());

    EXPECT_FLOAT_EQ(dot.GetGroundOffset(), 0.0f);
    dot.SetGroundOffset(1.5f);
    EXPECT_FLOAT_EQ(dot.GetGroundOffset(), 1.5f);
}
