#include "dual_contouring.h"
#include <vector>
#include <array>
#include <tuple>
#include <iostream>
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

        int nx = static_cast<int>(std::ceil((max_bound.x - min_bound.x) / cell_size));
        int ny = static_cast<int>(std::ceil((max_bound.y - min_bound.y) / cell_size));
        int nz = static_cast<int>(std::ceil((max_bound.z - min_bound.z) / cell_size));

        // Safety check to prevent OOM lockups
        if (nx <= 0 || ny <= 0 || nz <= 0 || nx > 512 || ny > 512 || nz > 512) {
            return mesh;
        }

        int corners_x = nx + 1;
        int corners_y = ny + 1;
        int corners_z = nz + 1;

        // 1. Pre-sample the field at grid corners
        // This dramatically reduces noise evaluations (1 sample per corner vs ~17 per cell)
        std::vector<float> field(corners_x * corners_y * corners_z);
        for (int z = 0; z < corners_z; ++z) {
            for (int y = 0; y < corners_y; ++y) {
                for (int x = 0; x < corners_x; ++x) {
                    glm::vec3 p = min_bound + glm::vec3(x, y, z) * cell_size;
                    field[x + y * corners_x + z * corners_x * corners_y] = sdf(p);
                }
            }
        }

        auto get_field = [&](int x, int y, int z) {
            // Clamp to valid range for boundary cells
            x = std::clamp(x, 0, corners_x - 1);
            y = std::clamp(y, 0, corners_y - 1);
            z = std::clamp(z, 0, corners_z - 1);
            return field[x + y * corners_x + z * corners_x * corners_y];
        };

        auto get_grad = [&](const glm::vec3& p) {
            if (grad) return grad(p);
            float eps = cell_size * 0.1f;
            glm::vec3 g(
                sdf(p + glm::vec3(eps, 0, 0)) - sdf(p - glm::vec3(eps, 0, 0)),
                sdf(p + glm::vec3(0, eps, 0)) - sdf(p - glm::vec3(0, eps, 0)),
                sdf(p + glm::vec3(0, 0, eps)) - sdf(p - glm::vec3(0, 0, eps))
            );
            float len = glm::length(g);
            return len > 1e-6f ? g / len : glm::vec3(0, 1, 0);
        };

        // 2. Vertex Generation: One vertex per cell crossing the surface
        // Extended grid: cells from -1 to n (inclusive) to capture boundary vertices
        int ext_nx = nx + 2;  // -1 to nx
        int ext_ny = ny + 2;  // -1 to ny
        int ext_nz = nz + 2;  // -1 to nz
        std::vector<int> grid(ext_nx * ext_ny * ext_nz, -1);

        auto grid_idx = [&](int x, int y, int z) {
            return (x + 1) + (y + 1) * ext_nx + (z + 1) * ext_nx * ext_ny;
        };

        // Process cells from -1 to n (inclusive)
        for (int z = -1; z <= nz; ++z) {
            for (int y = -1; y <= ny; ++y) {
                for (int x = -1; x <= nx; ++x) {
                    // Check 8 corners of the cell for sign change (with clamping)
                    bool has_inside = false;
                    bool has_outside = false;
                    for (int i = 0; i < 8; ++i) {
                        float v = get_field(x + (i & 1), y + ((i >> 1) & 1), z + ((i >> 2) & 1));
                        if (v < 0.0f) has_inside = true;
                        else has_outside = true;
                    }

                    if (has_inside && has_outside) {
                        std::vector<EdgeIntersection> intersections;
                        glm::vec3 cell_min = min_bound + glm::vec3(x, y, z) * cell_size;

                        // Check 12 edges for intersections
                        static const int edge_offsets[12][2][3] = {
                            {{0,0,0},{1,0,0}}, {{0,1,0},{1,1,0}}, {{0,0,1},{1,0,1}}, {{0,1,1},{1,1,1}},
                            {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{0,0,1},{0,1,1}}, {{1,0,1},{1,1,1}},
                            {{0,0,0},{0,0,1}}, {{1,0,0},{1,0,1}}, {{0,1,0},{0,1,1}}, {{1,1,0},{1,1,1}}
                        };

                        for (int i = 0; i < 12; ++i) {
                            int cx1 = x + edge_offsets[i][0][0], cy1 = y + edge_offsets[i][0][1], cz1 = z + edge_offsets[i][0][2];
                            int cx2 = x + edge_offsets[i][1][0], cy2 = y + edge_offsets[i][1][1], cz2 = z + edge_offsets[i][1][2];
                            float v1 = get_field(cx1, cy1, cz1);
                            float v2 = get_field(cx2, cy2, cz2);

                            if ((v1 < 0.0f) != (v2 < 0.0f)) {
                                float t = -v1 / (v2 - v1);
                                glm::vec3 p1 = min_bound + glm::vec3(cx1, cy1, cz1) * cell_size;
                                glm::vec3 p2 = min_bound + glm::vec3(cx2, cy2, cz2) * cell_size;
                                glm::vec3 p = p1 + t * (p2 - p1);
                                intersections.push_back({p, get_grad(p)});
                            }
                        }

                        if (!intersections.empty()) {
                            glm::vec3 center = cell_min + glm::vec3(0.5f) * cell_size;
                            glm::vec3 v_pos = SolveQEF(intersections, center);

                            // Keep vertex within cell bounds to avoid mesh intersections
                            v_pos = glm::clamp(v_pos, cell_min, cell_min + glm::vec3(cell_size));

                            glm::vec3 avg_norm(0);
                            for (auto& intersect : intersections) avg_norm += intersect.n;
                            // Negate for interior cave walls
                            avg_norm = -glm::normalize(avg_norm);

                            grid[grid_idx(x, y, z)] = static_cast<int>(mesh.vertices.size());
                            mesh.vertices.push_back({v_pos, avg_norm});
                        }
                    }
                }
            }
        }

        // 3. Index Generation: One quad per edge crossing the surface
        auto get_v = [&](int x, int y, int z) -> int {
            // Extended grid allows cells from -1 to n
            if (x < -1 || x > nx || y < -1 || y > ny || z < -1 || z > nz) return -1;
            return grid[grid_idx(x, y, z)];
        };

        for (int z = 0; z < corners_z; ++z) {
            for (int y = 0; y < corners_y; ++y) {
                for (int x = 0; x < corners_x; ++x) {
                    // Check +X, +Y, +Z edges starting from this corner
                    if (x < nx) { // X-edge
                        float v1 = get_field(x, y, z), v2 = get_field(x+1, y, z);
                        if ((v1 < 0.0f) != (v2 < 0.0f)) {
                            int v_ids[4] = {get_v(x,y,z), get_v(x,y-1,z), get_v(x,y-1,z-1), get_v(x,y,z-1)};
                            if (v_ids[0]!=-1 && v_ids[1]!=-1 && v_ids[2]!=-1 && v_ids[3]!=-1) {
                                bool rev = (v1 < 0.0f);
                                if (rev) {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[1]); mesh.indices.push_back(v_ids[2]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[3]);
                                } else {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[1]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[3]); mesh.indices.push_back(v_ids[2]);
                                }
                            }
                        }
                    }
                    if (y < ny) { // Y-edge
                        float v1 = get_field(x, y, z), v2 = get_field(x, y+1, z);
                        if ((v1 < 0.0f) != (v2 < 0.0f)) {
                            int v_ids[4] = {get_v(x,y,z), get_v(x,y,z-1), get_v(x-1,y,z-1), get_v(x-1,y,z)};
                            if (v_ids[0]!=-1 && v_ids[1]!=-1 && v_ids[2]!=-1 && v_ids[3]!=-1) {
                                bool rev = (v1 < 0.0f); // Consistent with X and Z axes
                                if (rev) {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[1]); mesh.indices.push_back(v_ids[2]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[3]);
                                } else {
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[2]); mesh.indices.push_back(v_ids[1]);
                                    mesh.indices.push_back(v_ids[0]); mesh.indices.push_back(v_ids[3]); mesh.indices.push_back(v_ids[2]);
                                }
                            }
                        }
                    }
                    if (z < nz) { // Z-edge
                        float v1 = get_field(x, y, z), v2 = get_field(x, y, z+1);
                        if ((v1 < 0.0f) != (v2 < 0.0f)) {
                            int v_ids[4] = {get_v(x,y,z), get_v(x-1,y,z), get_v(x-1,y-1,z), get_v(x,y-1,z)};
                            if (v_ids[0]!=-1 && v_ids[1]!=-1 && v_ids[2]!=-1 && v_ids[3]!=-1) {
                                bool rev = (v1 < 0.0f);
                                if (rev) {
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
        glm::vec3 centroid(0);
        for (const auto& i : intersections) centroid += i.p;
        centroid /= static_cast<float>(intersections.size());

        glm::mat3 AtA(0);
        glm::vec3 Atb(0);

        for (const auto& intersect : intersections) {
            // Stability: Solve relative to centroid
            glm::vec3 p = intersect.p - centroid;
            AtA[0][0] += intersect.n.x * intersect.n.x;
            AtA[0][1] += intersect.n.x * intersect.n.y;
            AtA[0][2] += intersect.n.x * intersect.n.z;
            AtA[1][1] += intersect.n.y * intersect.n.y;
            AtA[1][2] += intersect.n.y * intersect.n.z;
            AtA[2][2] += intersect.n.z * intersect.n.z;

            float b = glm::dot(intersect.n, p);
            Atb += intersect.n * b;
        }

        AtA[1][0] = AtA[0][1];
        AtA[2][0] = AtA[0][2];
        AtA[2][1] = AtA[1][2];

        // Regularization: pulls toward centroid
        float lambda = 0.1f;
        AtA[0][0] += lambda;
        AtA[1][1] += lambda;
        AtA[2][2] += lambda;

        float det = glm::determinant(AtA);
        if (std::abs(det) > 1e-6f) {
            glm::vec3 sol = glm::inverse(AtA) * Atb;
            return sol + centroid;
        }

        return centroid;
    }

}
