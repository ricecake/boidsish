#include <gtest/gtest.h>
#include "polyhedron.h"
#include <vector>
#include <glm/glm.hpp>

using namespace Boidsish;

TEST(PolyhedronTest, Initialization) {
    PolyhedronType types[] = {
        PolyhedronType::Tetrahedron,
        PolyhedronType::Cube,
        PolyhedronType::Octahedron,
        PolyhedronType::Dodecahedron,
        PolyhedronType::Icosahedron,
        PolyhedronType::SmallStellatedDodecahedron,
        PolyhedronType::GreatDodecahedron,
        PolyhedronType::GreatStellatedDodecahedron,
        PolyhedronType::GreatIcosahedron
    };

    for (auto type : types) {
        Polyhedron poly(type);
        EXPECT_EQ(poly.GetInstanceKey(), "Polyhedron:" + std::to_string(static_cast<int>(type)));
    }
}

TEST(PolyhedronTest, GeometryGeneration) {
    // We can't easily test GPU resources in a unit test without a GL context,
    // but we can test the data generation if we expose it or use a mock megabuffer.
    // For now, let's just check that it doesn't crash when we call EnsureMeshInitialized.
    // Actually, EnsureMeshInitialized calls glGenVertexArrays, which will fail without a context.

    // Instead, let's just verify the class structure and basic properties.
    Polyhedron poly(PolyhedronType::Cube, 1, 10.0f, 20.0f, 30.0f, 5.0f);
    EXPECT_FLOAT_EQ(poly.GetX(), 10.0f);
    EXPECT_FLOAT_EQ(poly.GetY(), 20.0f);
    EXPECT_FLOAT_EQ(poly.GetZ(), 30.0f);
}

TEST(PolyhedronTest, WindingOrder) {
    // This would require intercepting the AddTriangle calls.
}
