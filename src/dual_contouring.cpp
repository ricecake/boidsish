#include "dual_contouring.h"
#include <map>
#include <array>
#include <tuple>

namespace Boidsish {

    DualContouringMesh DualContouring::Generate(
        const glm::vec3& min_bound,
        const glm::vec3& max_bound,
        float cell_size,
        SDFunction sdf,
        GradFunction grad
    ) {
        DualContouringMesh mesh;
        std::map<std::tuple<int, int, int>, unsigned int> grid;

        int nx = static_cast<int>((max_bound.x - min_bound.x) / cell_size);
        int ny = static_cast<int>((max_bound.y - min_bound.y) / cell_size);
        int nz = static_cast<int>((max_bound.z - min_bound.z) / cell_size);

        auto get_grad = [&](const glm::vec3& p) {
            if (grad) return grad(p);
            float eps = 0.01f;
            return glm::normalize(glm::vec3(
                sdf(p + glm::vec3(eps, 0, 0)) - sdf(p - glm::vec3(eps, 0, 0)),
                sdf(p + glm::vec3(0, eps, 0)) - sdf(p - glm::vec3(0, eps, 0)),
                sdf(p + glm::vec3(0, 0, eps)) - sdf(p - glm::vec3(0, 0, eps))
            ));
        };

        // 1. For each cell, find if it contains a surface and calculate optimal vertex
        for (int x = 0; x < nx; ++x) {
            for (int y = 0; y < ny; ++y) {
                for (int z = 0; z < nz; ++z) {
                    glm::vec3 cell_min = min_bound + glm::vec3(x, y, z) * cell_size;
                    std::vector<EdgeIntersection> intersections;

                    // Check 12 edges of the cell
                    static const int edges[12][2][3] = {
                        {{0,0,0},{1,0,0}}, {{0,1,0},{1,1,0}}, {{0,0,1},{1,0,1}}, {{0,1,1},{1,1,1}},
                        {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{0,0,1},{0,1,1}}, {{1,0,1},{1,1,1}},
                        {{0,0,0},{0,0,1}}, {{1,0,0},{1,0,1}}, {{0,1,0},{0,1,1}}, {{1,1,0},{1,1,1}}
                    };

                    for (int i = 0; i < 12; ++i) {
                        glm::vec3 p1 = cell_min + glm::vec3(edges[i][0][0], edges[i][0][1], edges[i][0][2]) * cell_size;
                        glm::vec3 p2 = cell_min + glm::vec3(edges[i][1][0], edges[i][1][1], edges[i][1][2]) * cell_size;
                        float v1 = sdf(p1);
                        float v2 = sdf(p2);

                        if ((v1 < 0) != (v2 < 0)) {
                            float t = -v1 / (v2 - v1);
                            glm::vec3 p = p1 + t * (p2 - p1);
                            intersections.push_back({p, get_grad(p)});
                        }
                    }

                    if (!intersections.empty()) {
                        glm::vec3 center = cell_min + glm::vec3(0.5f) * cell_size;
                        glm::vec3 v_pos = SolveQEF(intersections, center);

                        // Keep within cell
                        v_pos = glm::clamp(v_pos, cell_min, cell_min + glm::vec3(cell_size));

                        glm::vec3 avg_norm(0);
                        for (auto& intersect : intersections) avg_norm += intersect.n;
                        avg_norm = glm::normalize(avg_norm);

                        grid[std::make_tuple(x, y, z)] = static_cast<unsigned int>(mesh.vertices.size());
                        mesh.vertices.push_back({v_pos, avg_norm});
                    }
                }
            }
        }

        // 2. For each edge, if it crosses the surface, create two triangles
        for (int x = 0; x < nx - 1; ++x) {
            for (int y = 0; y < ny - 1; ++y) {
                for (int z = 0; z < nz - 1; ++z) {
                    float v = sdf(min_bound + glm::vec3(x, y, z) * cell_size);
                    float vx = sdf(min_bound + glm::vec3(x+1, y, z) * cell_size);
                    float vy = sdf(min_bound + glm::vec3(x, y+1, z) * cell_size);
                    float vz = sdf(min_bound + glm::vec3(x, y, z+1) * cell_size);

                    auto add_quad = [&](std::array<std::tuple<int,int,int>, 4> cells, bool flip) {
                        unsigned int ids[4];
                        for(int i=0; i<4; ++i) {
                            auto it = grid.find(cells[i]);
                            if (it == grid.end()) return;
                            ids[i] = it->second;
                        }
                        if (flip) {
                            mesh.indices.push_back(ids[0]); mesh.indices.push_back(ids[1]); mesh.indices.push_back(ids[2]);
                            mesh.indices.push_back(ids[0]); mesh.indices.push_back(ids[2]); mesh.indices.push_back(ids[3]);
                        } else {
                            mesh.indices.push_back(ids[0]); mesh.indices.push_back(ids[2]); mesh.indices.push_back(ids[1]);
                            mesh.indices.push_back(ids[0]); mesh.indices.push_back(ids[3]); mesh.indices.push_back(ids[2]);
                        }
                    };

                    if ((v < 0) != (vx < 0)) {
                        add_quad({std::make_tuple(x,y,z), std::make_tuple(x,y-1,z), std::make_tuple(x,y-1,z-1), std::make_tuple(x,y,z-1)}, (v < 0));
                    }
                    if ((v < 0) != (vy < 0)) {
                        add_quad({std::make_tuple(x,y,z), std::make_tuple(x-1,y,z), std::make_tuple(x-1,y,z-1), std::make_tuple(x,y,z-1)}, (v > 0));
                    }
                    if ((v < 0) != (vz < 0)) {
                        add_quad({std::make_tuple(x,y,z), std::make_tuple(x-1,y,z), std::make_tuple(x-1,y-1,z), std::make_tuple(x,y-1,z)}, (v < 0));
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

            float d = glm::dot(intersect.n, intersect.n); // Weighting? Or just 1.0 if normalized
            (void)d;
            float b = glm::dot(intersect.n, intersect.p);
            Atb += intersect.n * b;
            centroid += intersect.p;
        }

        AtA[1][0] = AtA[0][1];
        AtA[2][0] = AtA[0][2];
        AtA[2][1] = AtA[1][2];

        centroid /= static_cast<float>(intersections.size());

        // Regularization to handle singular matrices
        float lambda = 0.01f;
        AtA[0][0] += lambda;
        AtA[1][1] += lambda;
        AtA[2][2] += lambda;

        // Solve AtA * x = Atb using Cramer's rule or direct inversion
        float det = glm::determinant(AtA);
        if (std::abs(det) > 1e-6f) {
            return glm::inverse(AtA) * Atb;
        }

        return centroid;
    }

}
