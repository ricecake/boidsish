#include <gtest/gtest.h>
#include "mesh_optimizer_util.h"
#include "geometry.h"
#include <vector>
#include <glm/glm.hpp>

using namespace Boidsish;

TEST(MeshOptimizerTest, OptimizeBasicMesh) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Create a simple quad (two triangles)
    vertices.push_back({glm::vec3(0,0,0), glm::vec3(0,0,1), glm::vec2(0,0)});
    vertices.push_back({glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec2(1,0)});
    vertices.push_back({glm::vec3(1,1,0), glm::vec3(0,0,1), glm::vec2(1,1)});
    vertices.push_back({glm::vec3(0,1,0), glm::vec3(0,0,1), glm::vec2(0,1)});

    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(0); indices.push_back(2); indices.push_back(3);

    size_t original_vertex_count = vertices.size();
    size_t original_index_count = indices.size();

    MeshOptimizerUtil::Optimize(vertices, indices, "test_quad");

    EXPECT_EQ(vertices.size(), original_vertex_count);
    EXPECT_EQ(indices.size(), original_index_count);

    // Check if indices are still valid
    for (unsigned int idx : indices) {
        EXPECT_LT(idx, vertices.size());
    }
}

TEST(MeshOptimizerTest, GenerateShadowIndices) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Create a cube with duplicated vertices for hard normals
    // 8 positions, but 24 vertices
    for (int i = 0; i < 8; ++i) {
        glm::vec3 pos((i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f, (i & 4) ? 1.0f : 0.0f);
        // Add 3 versions of each position with different normals
        vertices.push_back({pos, glm::vec3(1,0,0), glm::vec2(0,0)});
        vertices.push_back({pos, glm::vec3(0,1,0), glm::vec2(0,0)});
        vertices.push_back({pos, glm::vec3(0,0,1), glm::vec2(0,0)});
    }

    // Just some dummy triangles
    indices = {0, 3, 6, 9, 12, 15};

    std::vector<unsigned int> shadow_indices;
    MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);

    EXPECT_EQ(shadow_indices.size(), indices.size());

    // Check if indices are remapped.
    // For example, vertex 1 and vertex 2 have the same position as vertex 0.
    // meshopt_generateShadowIndexBuffer should remap indices pointing to 1 or 2 to point to 0.

    // In our case, 0, 3, 6, 9, 12, 15 all have different positions, so they might not be merged.
    // Let's add indices that we KNOW should be merged.
    indices = {0, 1, 2}; // All three refer to the same position: pos of i=0
    MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);

    EXPECT_EQ(shadow_indices[0], shadow_indices[1]);
    EXPECT_EQ(shadow_indices[0], shadow_indices[2]);

    // All should point to index 0 (the first vertex with that position)
    EXPECT_EQ(shadow_indices[0], 0);
}

TEST(MeshOptimizerTest, SimplifyMesh) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Create a slightly more complex mesh: a grid of 2x2 quads (8 triangles, 9 vertices)
    for (int y = 0; y <= 2; ++y) {
        for (int x = 0; x <= 2; ++x) {
            vertices.push_back({glm::vec3(x, y, 0), glm::vec3(0, 0, 1), glm::vec2(x/2.0f, y/2.0f)});
        }
    }

    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            unsigned int start = y * 3 + x;
            indices.push_back(start); indices.push_back(start + 1); indices.push_back(start + 3);
            indices.push_back(start + 1); indices.push_back(start + 4); indices.push_back(start + 3);
        }
    }

    size_t original_index_count = indices.size();

    // Simplify with 1% error
    MeshOptimizerUtil::Simplify(vertices, indices, 0.01f, 0.5f, 0, "test_grid");

    EXPECT_LE(indices.size(), original_index_count);

    // Check if indices are still valid
    for (unsigned int idx : indices) {
        EXPECT_LT(idx, vertices.size());
    }
}
