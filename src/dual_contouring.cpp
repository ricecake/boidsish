#include "dual_contouring.h"
#include <vector>
#include <array>
#include <tuple>
#include <glm/gtc/matrix_inverse.hpp>

namespace Boidsish {

    DualContouringMesh DualContouring::Generate(
        const glm::vec3& min_bound,
        const glm::vec3& max_bound,
        float cell_size,
        SDFunction sdf,
        GradFunction grad
    ) {
        DualContouringMesh mesh;

        int nx = static_cast<int>((max_bound.x - min_bound.x) / cell_size);
        int ny = static_cast<int>((max_bound.y - min_bound.y) / cell_size);
        int nz = static_cast<int>((max_bound.z - min_bound.z) / cell_size);

        if (nx <= 0 || ny <= 0 || nz <= 0) return mesh;

        // Grid stores vertex index for each cell
        std::vector<int> grid(nx * ny * nz, -1);

        auto get_grad = [&](const glm::vec3& p) {
            if (grad) return grad(p);
            // Slightly larger epsilon for more stable finite differences
            float eps = cell_size * 0.05f;
            glm::vec3 g(
                sdf(p + glm::vec3(eps, 0, 0)) - sdf(p - glm::vec3(eps, 0, 0)),
                sdf(p + glm::vec3(0, eps, 0)) - sdf(p - glm::vec3(0, eps, 0)),
                sdf(p + glm::vec3(0, 0, eps)) - sdf(p - glm::vec3(0, 0, eps))
            );
            float len = glm::length(g);
            return len > 1e-6f ? g / len : glm::vec3(0, 1, 0);
        };

        // 1. Vertex Generation: One vertex per cell containing the surface
        for (int z = 0; z < nz; ++z) {
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    glm::vec3 cell_min = min_bound + glm::vec3(x, y, z) * cell_size;
                    std::vector<EdgeIntersection> intersections;

                    // Check 12 edges of the current cell
                    static const int edge_offsets[12][2][3] = {
                        {{0,0,0},{1,0,0}}, {{0,1,0},{1,1,0}}, {{0,0,1},{1,0,1}}, {{0,1,1},{1,1,1}},
                        {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{0,0,1},{0,1,1}}, {{1,0,1},{1,1,1}},
                        {{0,0,0},{0,0,1}}, {{1,0,0},{1,0,1}}, {{0,1,0},{0,1,1}}, {{1,1,0},{1,1,1}}
                    };

                    for (int i = 0; i < 12; ++i) {
                        glm::vec3 p1 = cell_min + glm::vec3(edge_offsets[i][0][0], edge_offsets[i][0][1], edge_offsets[i][0][2]) * cell_size;
                        glm::vec3 p2 = cell_min + glm::vec3(edge_offsets[i][1][0], edge_offsets[i][1][1], edge_offsets[i][1][2]) * cell_size;
                        float v1 = sdf(p1);
                        float v2 = sdf(p2);

                        if ((v1 < 0.0f) != (v2 < 0.0f)) {
                            float t = -v1 / (v2 - v1);
                            glm::vec3 p = p1 + t * (p2 - p1);
                            intersections.push_back({p, get_grad(p)});
                        }
                    }

                    if (!intersections.empty()) {
                        glm::vec3 center = cell_min + glm::vec3(0.5f) * cell_size;
                        glm::vec3 v_pos = SolveQEF(intersections, center);

                        // Keep vertex within cell bounds to avoid non-manifold geometry
                        v_pos = glm::clamp(v_pos, cell_min, cell_min + glm::vec3(cell_size));

                        glm::vec3 avg_norm(0);
                        for (auto& intersect : intersections) avg_norm += intersect.n;
                        // For interior meshing (caves), normals should point into the air/negative SDF
                        avg_norm = -glm::normalize(avg_norm);

                        grid[x + y * nx + z * nx * ny] = static_cast<int>(mesh.vertices.size());
                        mesh.vertices.push_back({v_pos, avg_norm});
                    }
                }
            }
        }

        // 2. Index Generation: One quad per edge crossing the surface
        auto get_v = [&](int x, int y, int z) -> int {
            if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz) return -1;
            return grid[x + y * nx + z * nx * ny];
        };

        // We check every edge in the grid. An edge is defined by its low corner and axis.
        // We only generate quads for edges that are shared by 4 valid cells.
        for (int z = 0; z < nz; ++z) {
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    glm::vec3 p = min_bound + glm::vec3(x, y, z) * cell_size;
                    float v = sdf(p);

                    // Check +X, +Y, +Z edges
                    for (int axis = 0; axis < 3; ++axis) {
                        glm::vec3 p_next = p;
                        if (axis == 0) p_next.x += cell_size;
                        else if (axis == 1) p_next.y += cell_size;
                        else p_next.z += cell_size;

                        float v_next = sdf(p_next);
                        if ((v < 0.0f) != (v_next < 0.0f)) {
                            int v_ids[4];
                            // If crossing from inside (v < 0) to outside (v_next > 0),
                            // winding 0-1-2-3 is CW from outside, which is CCW from inside.
                            bool reverse = (v < 0.0f);

                            if (axis == 0) { // X-edge
                                v_ids[0] = get_v(x, y, z);
                                v_ids[1] = get_v(x, y - 1, z);
                                v_ids[2] = get_v(x, y - 1, z - 1);
                                v_ids[3] = get_v(x, y, z - 1);
                            } else if (axis == 1) { // Y-edge
                                v_ids[0] = get_v(x, y, z);
                                v_ids[1] = get_v(x, y, z - 1);
                                v_ids[2] = get_v(x - 1, y, z - 1);
                                v_ids[3] = get_v(x - 1, y, z);
                            } else { // Z-edge
                                v_ids[0] = get_v(x, y, z);
                                v_ids[1] = get_v(x - 1, y, z);
                                v_ids[2] = get_v(x - 1, y - 1, z);
                                v_ids[3] = get_v(x, y - 1, z);
                            }

                            // Only add if all 4 adjacent cells have surface vertices
                            bool valid = true;
                            for (int i = 0; i < 4; ++i) if (v_ids[i] == -1) valid = false;

                            if (valid) {
                                if (reverse) {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[1]); mesh.indices.push_back(v_ids[2]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[3]);
                                } else {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[1]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[3]); mesh.indices.push_back(v_ids[2]);
                                }
                            }
                        }
                    }
                }
            }
        }

        return mesh;
    }

    glm::vec3 DualContouring::SolveQEF(const std::vector<EdgeIntersection>& intersections, const glm::vec3& cell_center) {
        glm::mat3 AtA(0);
        glm::vec3 Atb(0);
        glm::vec3 centroid(0);

        for (const auto& intersect : intersections) {
            AtA[0][0] += intersect.n.x * intersect.n.x;
            AtA[0][1] += intersect.n.x * intersect.n.y;
            AtA[0][2] += intersect.n.x * intersect.n.z;
            AtA[1][1] += intersect.n.y * intersect.n.y;
            AtA[1][2] += intersect.n.y * intersect.n.z;
            AtA[2][2] += intersect.n.z * intersect.n.z;

            float b = glm::dot(intersect.n, intersect.p);
            Atb += intersect.n * b;
            centroid += intersect.p;
        }

        AtA[1][0] = AtA[0][1];
        AtA[2][0] = AtA[0][2];
        AtA[2][1] = AtA[1][2];

        centroid /= static_cast<float>(intersections.size());

        // Regularization to handle near-singular cases (common in organic smooth SDFs)
        float lambda = 0.1f;
        AtA[0][0] += lambda;
        AtA[1][1] += lambda;
        AtA[2][2] += lambda;

        float det = glm::determinant(AtA);
        if (std::abs(det) > 1e-6f) {
            glm::vec3 sol = glm::inverse(AtA) * Atb;
            // If the solution is way outside the cell, the QEF is likely unstable
            // (even with regularization). Use centroid instead for smoother look.
            if (glm::distance(sol, centroid) > 1.5f) {
                return centroid;
            }
            return sol;
        }

        return centroid;
    }

}
