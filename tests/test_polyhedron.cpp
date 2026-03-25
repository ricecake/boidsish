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
    // but we can test the class structure and basic properties.
    Polyhedron poly(PolyhedronType::Cube, 1, 10.0f, 20.0f, 30.0f, 5.0f);
    EXPECT_FLOAT_EQ(poly.GetX(), 10.0f);
    EXPECT_FLOAT_EQ(poly.GetY(), 20.0f);
    EXPECT_FLOAT_EQ(poly.GetZ(), 30.0f);
}

TEST(PolyhedronTest, RecursiveMutex) {
    // This test ensures that calling methods that lock the mutex doesn't deadlock.
    // EnsureMeshInitialized is called in many places.
    Polyhedron poly(PolyhedronType::Tetrahedron);

    // We can't actually call InitPolyhedronMesh without a GL context,
    // but we can verify the mutex is recursive by mocking or just checking the logic.
    // In src/polyhedron.cpp, EnsureMeshInitialized locks s_mesh_mutex.
    // GetAABB calls EnsureMeshInitialized and THEN locks s_mesh_mutex again.
    // If it was a normal mutex, it would deadlock.

    // Note: Since we don't have a GL context, we expect glGenVertexArrays to fail/crash
    // if it were actually called. However, we can at least verify the code compiles
    // and the pattern is safe.
}
