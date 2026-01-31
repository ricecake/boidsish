#include <gtest/gtest.h>
#include "spline.h"

using namespace Boidsish;

TEST(SplineTest, CatmullRomEndpoints) {
    Vector3 p0(-1, 0, 0);
    Vector3 p1(0, 0, 0);
    Vector3 p2(1, 0, 0);
    Vector3 p3(2, 0, 0);

    // t=0 should be p1
    Vector3 result0 = Spline::CatmullRom(0.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result0.x, p1.x, 0.001f);
    EXPECT_NEAR(result0.y, p1.y, 0.001f);
    EXPECT_NEAR(result0.z, p1.z, 0.001f);

    // t=1 should be p2
    Vector3 result1 = Spline::CatmullRom(1.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result1.x, p2.x, 0.001f);
    EXPECT_NEAR(result1.y, p2.y, 0.001f);
    EXPECT_NEAR(result1.z, p2.z, 0.001f);
}

TEST(SplineTest, CatmullRomMidpoint) {
    Vector3 p0(-1, 0, 0);
    Vector3 p1(0, 0, 0);
    Vector3 p2(1, 0, 0);
    Vector3 p3(2, 0, 0);

    // t=0.5 should be (0.5, 0, 0) for a straight line
    Vector3 result = Spline::CatmullRom(0.5f, p0, p1, p2, p3);
    EXPECT_NEAR(result.x, 0.5f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
    EXPECT_NEAR(result.z, 0.0f, 0.001f);
}

TEST(SplineTest, CatmullRomDerivative) {
    Vector3 p0(-1, 0, 0);
    Vector3 p1(0, 0, 0);
    Vector3 p2(1, 0, 0);
    Vector3 p3(2, 0, 0);

    // For a straight line with uniform spacing, derivative should be constant (p2-p1) = (1,0,0)
    Vector3 deriv = Spline::CatmullRomDerivative(0.5f, p0, p1, p2, p3);
    EXPECT_NEAR(deriv.x, 1.0f, 0.001f);
    EXPECT_NEAR(deriv.y, 0.0f, 0.001f);
    EXPECT_NEAR(deriv.z, 0.0f, 0.001f);
}
