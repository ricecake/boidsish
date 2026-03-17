#include "sdf_utils.h"
#include "model.h"
#include "logger.h"
#include "tiny_bvh.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>

namespace Boidsish {

    VoxelGrid SdfUtils::VoxelizeModel(const Model& model, glm::uvec3 resolution) {
        auto model_data = model.GetData();
        if (!model_data) {
            logger::ERROR("SdfUtils: Model data is null");
            return VoxelGrid(resolution, glm::vec3(0), glm::vec3(0));
        }

        AABB aabb = model.GetAABB();
        // Add a small margin to avoid numerical issues at boundaries
        glm::vec3 size = aabb.max - aabb.min;
        aabb.min -= size * 0.05f;
        aabb.max += size * 0.05f;

        VoxelGrid grid(resolution, aabb.min, aabb.max);

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        model.GetGeometry(vertices, indices);

        if (indices.empty()) {
            logger::ERROR("SdfUtils: Model has no geometry");
            return grid;
        }

        // Prepare tinybvh
        std::vector<tinybvh::bvhvec4> bvh_verts;
        bvh_verts.reserve(indices.size());
        for (unsigned int idx : indices) {
            glm::vec3 p = vertices[idx].Position;
            bvh_verts.push_back(tinybvh::bvhvec4(p.x, p.y, p.z, 0.0f));
        }

        tinybvh::BVH bvh;
        bvh.Build(bvh_verts.data(), indices.size() / 3);

        logger::INFO("SdfUtils: Voxelizing {}x{}x{} grid...", resolution.x, resolution.y, resolution.z);

        // Parallel Voxelization
        #pragma omp parallel for collapse(3)
        for (int z = 0; z < (int)resolution.z; ++z) {
            for (int y = 0; y < (int)resolution.y; ++y) {
                for (int x = 0; x < (int)resolution.x; ++x) {
                    glm::vec3 center = grid.GetVoxelCenter(x, y, z);
                    int inside_votes = 0;
                    glm::vec3 ray_dirs[] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0.577f, 0.577f, 0.577f} };

                    for (const auto& dir : ray_dirs) {
                        tinybvh::Ray ray(
                            tinybvh::bvhvec3(center.x, center.y, center.z),
                            tinybvh::bvhvec3(dir.x, dir.y, dir.z)
                        );
                        int hits = 0;
                        while (true) {
                            bvh.Intersect(ray);
                            if (ray.hit.t >= BVH_FAR) break;

                            hits++;
                            float dist = ray.hit.t;
                            ray.O = ray.O + (dist + 0.0001f) * ray.D;
                            ray.hit.t = BVH_FAR;
                            if (hits > 100) break; // Safety break
                        }
                        if (hits % 2 != 0) inside_votes++;
                    }

                    if (inside_votes >= 2) {
                        // Mark as interior in a thread-safe way (grid.Set is okay if indices are unique)
                        grid.voxels[z * resolution.y * resolution.x + y * resolution.x + x] = 2;
                    }
                }
            }
        }

        // Refined surface detection
        std::vector<uint8_t> next_voxels = grid.voxels;
        #pragma omp parallel for collapse(3)
        for (int z = 0; z < (int)resolution.z; ++z) {
            for (int y = 0; y < (int)resolution.y; ++y) {
                for (int x = 0; x < (int)resolution.x; ++x) {
                    uint8_t current = grid.voxels[z * resolution.y * resolution.x + y * resolution.x + x];
                    bool on_boundary = false;
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                int nx = (int)x + dx;
                                int ny = (int)y + dy;
                                int nz = (int)z + dz;
                                if (nx >= 0 && nx < (int)resolution.x &&
                                    ny >= 0 && ny < (int)resolution.y &&
                                    nz >= 0 && nz < (int)resolution.z) {
                                    if (grid.voxels[nz * resolution.y * resolution.x + ny * resolution.x + nx] != current) {
                                        on_boundary = true;
                                        break;
                                    }
                                }
                            }
                            if (on_boundary) break;
                        }
                        if (on_boundary) break;
                    }
                    if (on_boundary) {
                        next_voxels[z * resolution.y * resolution.x + y * resolution.x + x] = 1;
                    }
                }
            }
        }
        grid.voxels = next_voxels;

        return grid;
    }

    SdfData SdfUtils::GenerateSDF(const VoxelGrid& grid) {
        SdfData sdf;
        sdf.res = grid.res;
        sdf.aabb_min = grid.aabb_min;
        sdf.aabb_max = grid.aabb_max;
        sdf.distances.resize(grid.res.x * grid.res.y * grid.res.z);

        // JFA implementation
        struct Seed {
            int16_t x, y, z;
        };

        std::vector<Seed> seeds(grid.res.x * grid.res.y * grid.res.z, { -1, -1, -1 });

        // Initialize seeds from surface voxels
        for (uint32_t z = 0; z < grid.res.z; ++z) {
            for (uint32_t y = 0; y < grid.res.y; ++y) {
                for (uint32_t x = 0; x < grid.res.x; ++x) {
                    if (grid.Get(x, y, z) == 1) {
                        seeds[z * grid.res.y * grid.res.x + y * grid.res.x + x] = { (int16_t)x, (int16_t)y, (int16_t)z };
                    }
                }
            }
        }

        auto get_seed = [&](int x, int y, int z) -> Seed {
            if (x < 0 || x >= (int)grid.res.x || y < 0 || y >= (int)grid.res.y || z < 0 || z >= (int)grid.res.z)
                return { -1, -1, -1 };
            return seeds[z * grid.res.y * grid.res.x + y * grid.res.x + x];
        };

        auto set_seed = [&](int x, int y, int z, Seed s) {
            seeds[z * grid.res.y * grid.res.x + y * grid.res.x + x] = s;
        };

        int max_dim = std::max({ (int)grid.res.x, (int)grid.res.y, (int)grid.res.z });
        int step = 1;
        while (step < max_dim) step *= 2;
        step /= 2;

        logger::INFO("SdfUtils: Starting JFA with max step {}...", step);

        std::vector<Seed> seeds_next = seeds;

        while (step >= 1) {
            #pragma omp parallel for collapse(3)
            for (int z = 0; z < (int)grid.res.z; ++z) {
                for (int y = 0; y < (int)grid.res.y; ++y) {
                    for (int x = 0; x < (int)grid.res.x; ++x) {
                        Seed best_s = seeds[z * grid.res.y * grid.res.x + y * grid.res.x + x];
                        float best_dist_sq = std::numeric_limits<float>::max();
                        if (best_s.x != -1) {
                            float dx = best_s.x - x;
                            float dy = best_s.y - y;
                            float dz = best_s.z - z;
                            best_dist_sq = dx * dx + dy * dy + dz * dz;
                        }

                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dx = -1; dx <= 1; ++dx) {
                                    if (dx == 0 && dy == 0 && dz == 0) continue;
                                    int nx = x + dx * step;
                                    int ny = y + dy * step;
                                    int nz = z + dz * step;

                                    if (nx >= 0 && nx < (int)grid.res.x &&
                                        ny >= 0 && ny < (int)grid.res.y &&
                                        nz >= 0 && nz < (int)grid.res.z) {
                                        Seed s = seeds[nz * grid.res.y * grid.res.x + ny * grid.res.x + nx];
                                        if (s.x != -1) {
                                            float diff_x = s.x - x;
                                            float diff_y = s.y - y;
                                            float diff_z = s.z - z;
                                            float d_sq = diff_x * diff_x + diff_y * diff_y + diff_z * diff_z;
                                            if (d_sq < best_dist_sq) {
                                                best_dist_sq = d_sq;
                                                best_s = s;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        seeds_next[z * grid.res.y * grid.res.x + y * grid.res.x + x] = best_s;
                    }
                }
            }
            seeds = seeds_next;
            step /= 2;
        }

        // Finalize distances and signs
        #pragma omp parallel for collapse(3)
        for (int z = 0; z < (int)grid.res.z; ++z) {
            for (int y = 0; y < (int)grid.res.y; ++y) {
                for (int x = 0; x < (int)grid.res.x; ++x) {
                    Seed s = seeds[z * grid.res.y * grid.res.x + y * grid.res.x + x];
                    float dist = 0.0f;
                    if (s.x != -1) {
                        glm::vec3 pos = grid.GetVoxelCenter(x, y, z);
                        glm::vec3 seed_pos = grid.GetVoxelCenter(s.x, s.y, s.z);
                        dist = glm::distance(pos, seed_pos);
                    } else {
                        // Should not happen if there are surface voxels
                        dist = 1e6f;
                    }

                    // Interior is negative
                    if (grid.Get(x, y, z) == 2 || grid.Get(x, y, z) == 1) {
                        // Note: surface voxels could be slightly inside or outside.
                        // For simplicity, let's treat the original "interior" as negative.
                        // Actually, let's cast a ray again to be sure of the sign if it's a surface voxel?
                        // Or just use the grid.voxels info.
                        if (grid.Get(x, y, z) == 2) dist = -dist;
                    }

                    sdf.distances[z * grid.res.y * grid.res.x + y * grid.res.x + x] = dist;
                }
            }
        }

        return sdf;
    }

    bool SdfUtils::SaveSDF(const SdfData& data, const std::string& path) {
        std::ofstream fs(path, std::ios::binary);
        if (!fs) return false;

        const char magic[4] = { 'A', 'S', 'D', 'F' };
        fs.write(magic, 4);

        uint32_t version = 1;
        fs.write((const char*)&version, sizeof(version));

        fs.write((const char*)&data.res, sizeof(data.res));
        fs.write((const char*)&data.aabb_min, sizeof(data.aabb_min));
        fs.write((const char*)&data.aabb_max, sizeof(data.aabb_max));

        fs.write((const char*)data.distances.data(), data.distances.size() * sizeof(float));

        return true;
    }

    bool SdfUtils::LoadSDF(SdfData& data, const std::string& path) {
        std::ifstream fs(path, std::ios::binary);
        if (!fs) return false;

        char magic[4];
        fs.read(magic, 4);
        if (magic[0] != 'A' || magic[1] != 'S' || magic[2] != 'D' || magic[3] != 'F') return false;

        uint32_t version;
        fs.read((char*)&version, sizeof(version));
        if (version != 1) return false;

        fs.read((char*)&data.res, sizeof(data.res));
        fs.read((char*)&data.aabb_min, sizeof(data.aabb_min));
        fs.read((char*)&data.aabb_max, sizeof(data.aabb_max));

        data.distances.resize(data.res.x * data.res.y * data.res.z);
        fs.read((char*)data.distances.data(), data.distances.size() * sizeof(float));

        return true;
    }

} // namespace Boidsish
