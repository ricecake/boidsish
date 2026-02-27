#include "procedural_mesher.h"
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <map>
#include "mesh_optimizer_util.h"
#include "ConfigManager.h"

namespace Boidsish {

    namespace {
        // Polynomial smooth minimum (union)
        float smin(float a, float b, float k, float& weight) {
            float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
            weight = h;
            return glm::mix(b, a, h) - k * h * (1.0f - h);
        }

        float sdCapsule(glm::vec3 p, glm::vec3 a, glm::vec3 b, float ra, float rb) {
            glm::vec3 pa = p - a, ba = b - a;
            float h = std::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
            return glm::length(pa - ba * h) - glm::mix(ra, rb, h);
        }

        float sdSphere(glm::vec3 p, glm::vec3 center, float s) {
            return glm::length(p - center) - s;
        }
    }

    float ProceduralMesher::SampleSDF(const glm::vec3& p, const ProceduralIR& ir, const SDFConfig& config, glm::vec3& out_color) {
        float min_dist = 1e10f;
        glm::vec3 blended_color(0.0f);
        float total_weight = 0.0f;

        for (const auto& e : ir.elements) {
            float d = 1e10f;
            if (e.type == ProceduralElementType::Tube) {
                d = sdCapsule(p, e.position, e.end_position, e.radius, e.end_radius);
            } else if (e.type == ProceduralElementType::Hub || e.type == ProceduralElementType::Puffball) {
                d = sdSphere(p, e.position, e.radius);
            } else {
                continue;
            }

            float w;
            if (min_dist > 1e9f) {
                min_dist = d;
                blended_color = e.color;
                total_weight = 1.0f;
            } else {
                min_dist = smin(min_dist, d, config.smooth_k, w);
                blended_color = glm::mix(blended_color, e.color, 1.0f - w);
            }
        }

        out_color = blended_color;
        return min_dist;
    }

    std::shared_ptr<Model> ProceduralMesher::GenerateModel(const ProceduralIR& ir) {
        if (ir.elements.empty()) return nullptr;

        SDFConfig config;
        config.grid_size = 0.1f; // Adjust for quality/speed
        config.smooth_k = 0.15f;

        // Calculate bounds
        glm::vec3 bmin(1e10f), bmax(-1e10f);
        for (const auto& e : ir.elements) {
            bmin = glm::min(bmin, e.position - glm::vec3(e.radius + config.grid_size * 2));
            bmax = glm::max(bmax, e.position + glm::vec3(e.radius + config.grid_size * 2));
            if (e.type == ProceduralElementType::Tube) {
                bmin = glm::min(bmin, e.end_position - glm::vec3(e.end_radius + config.grid_size * 2));
                bmax = glm::max(bmax, e.end_position + glm::vec3(e.end_radius + config.grid_size * 2));
            }
        }
        config.bounds_min = bmin;
        config.bounds_max = bmax;

        auto data = GenerateSurfaceNets(ir, config);

        // Add Leaves (which are 2D shapes, not easily SDF'd without thick volume)
        // We add them as separate meshes for simplicity, or we could SDF them too if we wanted.
        // For now, immediate geometry for leaves is fine and matches existing style.
        // Wait, IR has leaves. We should add them to the model data.

        std::vector<Vertex> leaf_vertices;
        std::vector<unsigned int> leaf_indices;

        auto AddLeafGeom = [&](glm::vec3 pos, glm::quat ori, float size, glm::vec3 color) {
            unsigned int base = leaf_vertices.size();
            std::vector<glm::vec3> pts = {{0, 0, 0}, {0.3f, 0.5f, 0.1f}, {0, 1.0f, 0}, {-0.3f, 0.5f, -0.1f}};
            for (auto& p : pts) {
                p = pos + ori * (p * size);
                Vertex v;
                v.Position = p;
                v.Normal = ori * glm::vec3(0, 0, 1);
                v.Color = color;
                v.TexCoords = glm::vec2(0.5f);
                leaf_vertices.push_back(v);
            }
            leaf_indices.push_back(base); leaf_indices.push_back(base + 1); leaf_indices.push_back(base + 2);
            leaf_indices.push_back(base); leaf_indices.push_back(base + 2); leaf_indices.push_back(base + 3);
            // Double sided
            leaf_indices.push_back(base); leaf_indices.push_back(base + 2); leaf_indices.push_back(base + 1);
            leaf_indices.push_back(base); leaf_indices.push_back(base + 3); leaf_indices.push_back(base + 2);
        };

        for (const auto& e : ir.elements) {
            if (e.type == ProceduralElementType::Leaf) {
                AddLeafGeom(e.position, e.orientation, e.radius, e.color);
            }
        }

        if (!leaf_vertices.empty()) {
            Boidsish::Mesh leaf_mesh(leaf_vertices, leaf_indices, {}, {});
            leaf_mesh.diffuseColor = {1, 1, 1};
            data->meshes.push_back(leaf_mesh);
        }

        return std::make_shared<Model>(data, true);
    }

    std::shared_ptr<ModelData> ProceduralMesher::GenerateSurfaceNets(const ProceduralIR& ir, const SDFConfig& config) {
        auto data = std::make_shared<ModelData>();
        data->model_path = "procedural_sdf_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        glm::ivec3 dims = glm::ivec3((config.bounds_max - config.bounds_min) / config.grid_size) + 1;
        auto get_pos = [&](int x, int y, int z) {
            return config.bounds_min + glm::vec3(x, y, z) * config.grid_size;
        };

        std::vector<float> grid(dims.x * dims.y * dims.z);
        std::vector<glm::vec3> color_grid(dims.x * dims.y * dims.z);

        for (int z = 0; z < dims.z; ++z) {
            for (int y = 0; y < dims.y; ++y) {
                for (int x = 0; x < dims.x; ++x) {
                    glm::vec3 color;
                    grid[x + y * dims.x + z * dims.x * dims.y] = SampleSDF(get_pos(x, y, z), ir, config, color);
                    color_grid[x + y * dims.x + z * dims.x * dims.y] = color;
                }
            }
        }

        std::map<int, unsigned int> vert_map;

        for (int z = 0; z < dims.z - 1; ++z) {
            for (int y = 0; y < dims.y - 1; ++y) {
                for (int x = 0; x < dims.x - 1; ++x) {
                    int corner_indices[8] = {
                        x + y * dims.x + z * dims.x * dims.y,
                        (x + 1) + y * dims.x + z * dims.x * dims.y,
                        (x + 1) + (y + 1) * dims.x + z * dims.x * dims.y,
                        x + (y + 1) * dims.x + z * dims.x * dims.y,
                        x + y * dims.x + (z + 1) * dims.x * dims.y,
                        (x + 1) + y * dims.x + (z + 1) * dims.x * dims.y,
                        (x + 1) + (y + 1) * dims.x + (z + 1) * dims.x * dims.y,
                        x + (y + 1) * dims.x + (z + 1) * dims.x * dims.y
                    };

                    int mask = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (grid[corner_indices[i]] < 0.0f) mask |= (1 << i);
                    }

                    if (mask == 0 || mask == 255) continue;

                    // Edge crossings
                    glm::vec3 vert_pos(0.0f);
                    glm::vec3 vert_color(0.0f);
                    int edge_count = 0;

                    const int edges[12][2] = {
                        {0,1}, {1,2}, {2,3}, {3,0},
                        {4,5}, {5,6}, {6,7}, {7,4},
                        {0,4}, {1,5}, {2,6}, {3,7}
                    };

                    for (int i = 0; i < 12; ++i) {
                        int i1 = edges[i][0];
                        int i2 = edges[i][1];
                        bool v1 = (mask & (1 << i1)) != 0;
                        bool v2 = (mask & (1 << i2)) != 0;
                        if (v1 != v2) {
                            float f1 = grid[corner_indices[i1]];
                            float f2 = grid[corner_indices[i2]];
                            float t = -f1 / (f2 - f1);
                            vert_pos += glm::mix(get_pos(x + (i1 & 1), y + ((i1 >> 1) & 1), z + ((i1 >> 2) & 1)),
                                                 get_pos(x + (i2 & 1), y + ((i2 >> 1) & 1), z + ((i2 >> 2) & 1)), t);
                            vert_color += glm::mix(color_grid[corner_indices[i1]], color_grid[corner_indices[i2]], t);
                            edge_count++;
                        }
                    }

                    if (edge_count > 0) {
                        Vertex v;
                        v.Position = vert_pos / (float)edge_count;
                        v.Color = vert_color / (float)edge_count;
                        v.Normal = glm::vec3(0, 0, 0); // Placeholder
                        v.TexCoords = glm::vec2(0.5f);
                        vert_map[x + y * dims.x + z * dims.x * dims.y] = (unsigned int)vertices.size();
                        vertices.push_back(v);
                    }
                }
            }
        }

        // Generate faces
        for (int z = 1; z < dims.z - 1; ++z) {
            for (int y = 1; y < dims.y - 1; ++y) {
                for (int x = 1; x < dims.x - 1; ++x) {
                    bool v0 = grid[x + y * dims.x + z * dims.x * dims.y] < 0.0f;

                    // X-axis edge
                    if (v0 != (grid[(x + 1) + y * dims.x + z * dims.x * dims.y] < 0.0f)) {
                        int quad[4] = {
                            x + y * dims.x + z * dims.x * dims.y,
                            x + (y - 1) * dims.x + z * dims.x * dims.y,
                            x + (y - 1) * dims.x + (z - 1) * dims.x * dims.y,
                            x + y * dims.x + (z - 1) * dims.x * dims.y
                        };
                        if (vert_map.count(quad[0]) && vert_map.count(quad[1]) && vert_map.count(quad[2]) && vert_map.count(quad[3])) {
                            if (v0) {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[1]]); indices.push_back(vert_map[quad[2]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[3]]);
                            } else {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[1]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[3]]); indices.push_back(vert_map[quad[2]]);
                            }
                        }
                    }
                    // Y-axis edge
                    if (v0 != (grid[x + (y + 1) * dims.x + z * dims.x * dims.y] < 0.0f)) {
                        int quad[4] = {
                            x + y * dims.x + z * dims.x * dims.y,
                            (x - 1) + y * dims.x + z * dims.x * dims.y,
                            (x - 1) + y * dims.x + (z - 1) * dims.x * dims.y,
                            x + y * dims.x + (z - 1) * dims.x * dims.y
                        };
                        if (vert_map.count(quad[0]) && vert_map.count(quad[1]) && vert_map.count(quad[2]) && vert_map.count(quad[3])) {
                            if (!v0) {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[1]]); indices.push_back(vert_map[quad[2]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[3]]);
                            } else {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[1]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[3]]); indices.push_back(vert_map[quad[2]]);
                            }
                        }
                    }
                    // Z-axis edge
                    if (v0 != (grid[x + y * dims.x + (z + 1) * dims.x * dims.y] < 0.0f)) {
                        int quad[4] = {
                            x + y * dims.x + z * dims.x * dims.y,
                            (x - 1) + y * dims.x + z * dims.x * dims.y,
                            (x - 1) + (y - 1) * dims.x + z * dims.x * dims.y,
                            x + (y - 1) * dims.x + z * dims.x * dims.y
                        };
                        if (vert_map.count(quad[0]) && vert_map.count(quad[1]) && vert_map.count(quad[2]) && vert_map.count(quad[3])) {
                            if (v0) {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[1]]); indices.push_back(vert_map[quad[2]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[3]]);
                            } else {
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[2]]); indices.push_back(vert_map[quad[1]]);
                                indices.push_back(vert_map[quad[0]]); indices.push_back(vert_map[quad[3]]); indices.push_back(vert_map[quad[2]]);
                            }
                        }
                    }
                }
            }
        }

        // Calculate Normals
        for (size_t i = 0; i < indices.size(); i += 3) {
            glm::vec3 v1 = vertices[indices[i]].Position;
            glm::vec3 v2 = vertices[indices[i+1]].Position;
            glm::vec3 v3 = vertices[indices[i+2]].Position;
            glm::vec3 n = glm::cross(v2 - v1, v3 - v1);
            vertices[indices[i]].Normal += n;
            vertices[indices[i+1]].Normal += n;
            vertices[indices[i+2]].Normal += n;
        }
        for (auto& v : vertices) {
            if (glm::length2(v.Normal) > 1e-6f)
                v.Normal = glm::normalize(v.Normal);
            else
                v.Normal = glm::vec3(0, 1, 0);
        }

        auto& config_mgr = ConfigManager::GetInstance();
        if (config_mgr.GetAppSettingBool("mesh_simplifier_enabled", false)) {
            MeshOptimizerUtil::Simplify(vertices, indices, 0.05f, 0.5f, 40, data->model_path);
        }

        std::vector<unsigned int> shadow_indices;
        if (config_mgr.GetAppSettingBool("mesh_optimizer_enabled", true)) {
            MeshOptimizerUtil::Optimize(vertices, indices, data->model_path);
            if (config_mgr.GetAppSettingBool("mesh_optimizer_shadow_indices_enabled", true)) {
                MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);
            }
        }

        Boidsish::Mesh mesh_obj(vertices, indices, {}, shadow_indices);
        mesh_obj.diffuseColor = {1, 1, 1};
        data->meshes.push_back(mesh_obj);

        if (!vertices.empty()) {
            glm::vec3 min = vertices[0].Position;
            glm::vec3 max = vertices[0].Position;
            for (const auto& v : vertices) {
                min = glm::min(min, v.Position);
                max = glm::max(max, v.Position);
            }
            data->aabb = AABB(min, max);
        }

        return data;
    }

} // namespace Boidsish
