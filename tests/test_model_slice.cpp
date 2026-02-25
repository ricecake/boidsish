#include <gtest/gtest.h>
#include "model.h"
#include <glm/glm.hpp>
#include <cstdlib>

using namespace Boidsish;

TEST(ModelSliceTest, GetRandomPoint) {
    ModelSlice slice;
    // Single triangle in XY plane
    slice.triangles.push_back(glm::vec3(0, 0, 0));
    slice.triangles.push_back(glm::vec3(1, 0, 0));
    slice.triangles.push_back(glm::vec3(0, 1, 0));

    // Seed rand for reproducibility in tests
    srand(42);

    // Pick many points and ensure they are within bounds
    for (int i = 0; i < 100; ++i) {
        glm::vec3 p = slice.GetRandomPoint();
        EXPECT_GE(p.x, -0.0001f);
        EXPECT_LE(p.x, 1.0001f);
        EXPECT_GE(p.y, -0.0001f);
        EXPECT_LE(p.y, 1.0001f);
        EXPECT_EQ(p.z, 0.0f);
        EXPECT_LE(p.x + p.y, 1.0001f); // Within the triangle
    }
}

TEST(ModelSliceTest, EmptySlice) {
    ModelSlice slice;
    glm::vec3 p = slice.GetRandomPoint();
    EXPECT_EQ(p.x, 0.0f);
    EXPECT_EQ(p.y, 0.0f);
    EXPECT_EQ(p.z, 0.0f);
}

TEST(ModelSliceTest, MultipleTriangles) {
    ModelSlice slice;
    // Triangle 1: Area 0.5
    slice.triangles.push_back(glm::vec3(0, 0, 0));
    slice.triangles.push_back(glm::vec3(1, 0, 0));
    slice.triangles.push_back(glm::vec3(0, 1, 0));

    // Triangle 2: Area 0.5, shifted
    slice.triangles.push_back(glm::vec3(10, 10, 10));
    slice.triangles.push_back(glm::vec3(11, 10, 10));
    slice.triangles.push_back(glm::vec3(10, 11, 10));

    bool hitT1 = false;
    bool hitT2 = false;

    for (int i = 0; i < 100; ++i) {
        glm::vec3 p = slice.GetRandomPoint();
        if (p.z == 0.0f) hitT1 = true;
        if (p.z == 10.0f) hitT2 = true;
    }

    EXPECT_TRUE(hitT1);
    EXPECT_TRUE(hitT2);
}

TEST(ModelSliceTest, ZeroAreaTriangle) {
    ModelSlice slice;
    // Degenerate triangle
    slice.triangles.push_back(glm::vec3(0, 0, 0));
    slice.triangles.push_back(glm::vec3(0, 0, 0));
    slice.triangles.push_back(glm::vec3(0, 0, 0));

    glm::vec3 p = slice.GetRandomPoint();
    EXPECT_EQ(p.x, 0.0f);
    EXPECT_EQ(p.y, 0.0f);
    EXPECT_EQ(p.z, 0.0f);
}
