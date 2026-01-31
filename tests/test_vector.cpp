#include <gtest/gtest.h>
#include "vector.h"

using namespace Boidsish;

TEST(Vector3Test, BasicOps) {
    Vector3 v1(1, 2, 3);
    Vector3 v2(4, 5, 6);

    Vector3 v3 = v1 + v2;
    EXPECT_FLOAT_EQ(v3.x, 5);
    EXPECT_FLOAT_EQ(v3.y, 7);
    EXPECT_FLOAT_EQ(v3.z, 9);

    Vector3 v4 = v2 - v1;
    EXPECT_FLOAT_EQ(v4.x, 3);
    EXPECT_FLOAT_EQ(v4.y, 3);
    EXPECT_FLOAT_EQ(v4.z, 3);

    Vector3 v5 = v1 * 2.0f;
    EXPECT_FLOAT_EQ(v5.x, 2);
    EXPECT_FLOAT_EQ(v5.y, 4);
    EXPECT_FLOAT_EQ(v5.z, 6);

    Vector3 v6 = v2 / 2.0f;
    EXPECT_FLOAT_EQ(v6.x, 2);
    EXPECT_FLOAT_EQ(v6.y, 2.5f);
    EXPECT_FLOAT_EQ(v6.z, 3);
}

TEST(Vector3Test, Methods) {
    Vector3 v(3, 4, 0);
    EXPECT_FLOAT_EQ(v.Magnitude(), 5.0f);
    EXPECT_FLOAT_EQ(v.MagnitudeSquared(), 25.0f);

    Vector3 vn = v.Normalized();
    EXPECT_FLOAT_EQ(vn.Magnitude(), 1.0f);
    EXPECT_FLOAT_EQ(vn.x, 0.6f);
    EXPECT_FLOAT_EQ(vn.y, 0.8f);

    Vector3 v1(1, 0, 0);
    Vector3 v2(0, 1, 0);
    EXPECT_FLOAT_EQ(v1.Dot(v2), 0.0f);

    Vector3 v3 = v1.Cross(v2);
    EXPECT_FLOAT_EQ(v3.x, 0);
    EXPECT_FLOAT_EQ(v3.y, 0);
    EXPECT_FLOAT_EQ(v3.z, 1);

    EXPECT_NEAR(v1.AngleTo(v2), 1.570796f, 0.0001f); // PI/2
}
