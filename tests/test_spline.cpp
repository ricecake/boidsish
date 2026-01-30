#include <gtest/gtest.h>
#include "spline.h"

using namespace Boidsish;

TEST(SplineTest, CatmullRom) {
    Vector3 p0(-1, 0, 0);
    Vector3 p1(0, 0, 0);
    Vector3 p2(1, 0, 0);
    Vector3 p3(2, 0, 0);

    // At t=0, it should be p1
    Vector3 result0 = Spline::CatmullRom(0.0f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(result0.x, 0.0f);
    EXPECT_FLOAT_EQ(result0.y, 0.0f);
    EXPECT_FLOAT_EQ(result0.z, 0.0f);

    // At t=1, it should be p2
    Vector3 result1 = Spline::CatmullRom(1.0f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(result1.x, 1.0f);
    EXPECT_FLOAT_EQ(result1.y, 0.0f);
    EXPECT_FLOAT_EQ(result1.z, 0.0f);

    // At t=0.5, it should be (0.5, 0, 0) for this linear setup
    Vector3 result05 = Spline::CatmullRom(0.5f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(result05.x, 0.5f);
}

TEST(SplineTest, Derivative) {
    Vector3 p0(-1, 0, 0);
    Vector3 p1(0, 0, 0);
    Vector3 p2(1, 0, 0);
    Vector3 p3(2, 0, 0);

    // For a straight line along X, derivative should be (1, 0, 0)
    Vector3 d = Spline::CatmullRomDerivative(0.5f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(d.x, 1.0f);
    EXPECT_FLOAT_EQ(d.y, 0.0f);
    EXPECT_FLOAT_EQ(d.z, 0.0f);
}
