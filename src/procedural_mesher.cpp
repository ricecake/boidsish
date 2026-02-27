#include "procedural_mesher.h"
#include "spline.h"
#include "mesh_optimizer_util.h"
#include "ConfigManager.h"
#include <numbers>

namespace Boidsish {

    namespace {
        const float SPLINE_RADIUS_SCALE = 0.005f;

        void GenerateUVSphere(
            std::vector<Vertex>& vertices,
            std::vector<unsigned int>& indices,
            glm::vec3 center, float radius, glm::vec3 color,
            int lat_segments, int lon_segments
        ) {
            unsigned int base = (unsigned int)vertices.size();

            for (int lat = 0; lat <= lat_segments; ++lat) {
                float theta = lat * (float)std::numbers::pi / lat_segments;
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);
                for (int lon = 0; lon <= lon_segments; ++lon) {
                    float phi = lon * 2.0f * (float)std::numbers::pi / lon_segments;
                    float sinPhi = std::sin(phi);
                    float cosPhi = std::cos(phi);

                    glm::vec3 normal(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);

                    Vertex v;
                    v.Position = center + normal * radius;
                    v.Normal = normal;
                    v.Color = color;
                    v.TexCoords = glm::vec2((float)lon / lon_segments, (float)lat / lat_segments);
                    vertices.push_back(v);
                }
            }

            for (int lat = 0; lat < lat_segments; ++lat) {
                for (int lon = 0; lon < lon_segments; ++lon) {
                    unsigned int first = base + lat * (lon_segments + 1) + lon;
                    unsigned int second = first + lon_segments + 1;

                    indices.push_back(first);
                    indices.push_back(first + 1);
                    indices.push_back(second);

                    indices.push_back(second);
                    indices.push_back(first + 1);
                    indices.push_back(second + 1);
                }
            }
        }
    }

    std::shared_ptr<Model> ProceduralMesher::GenerateModel(const ProceduralIR& ir) {
        if (ir.elements.empty()) return nullptr;

        auto data = GenerateDirectMesh(ir);

        // Add Leaf geometry as a separate mesh
        std::vector<Vertex> leaf_vertices;
        std::vector<unsigned int> leaf_indices;

        auto AddLeafGeom = [&](glm::vec3 pos, glm::quat ori, float size, glm::vec3 color) {
            unsigned int base = (unsigned int)leaf_vertices.size();
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

    std::shared_ptr<ModelData> ProceduralMesher::GenerateDirectMesh(const ProceduralIR& ir) {
        auto data = std::make_shared<ModelData>();
        data->model_path = "procedural_direct_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // === PHASE 1: Extract branches and generate tube geometry ===
        //
        // A branch is a chain of connected Tubes (with optional ControlPoints as
        // curve guides) that starts from a root element or a Hub child and ends
        // at a terminal element (Puffball, Leaf, Hub, or no children).

        // Find branch starting points: root tubes, or tubes that are children of Hubs
        std::vector<int> branch_starts;
        for (int i = 0; i < (int)ir.elements.size(); ++i) {
            const auto& e = ir.elements[i];
            if (e.type != ProceduralElementType::Tube) continue;

            if (e.parent == -1) {
                branch_starts.push_back(i);
            } else if (ir.elements[e.parent].type == ProceduralElementType::Hub) {
                branch_starts.push_back(i);
            }
        }

        for (int start_idx : branch_starts) {
            std::vector<Vector3> points;
            std::vector<Vector3> ups;
            std::vector<float> sizes;
            std::vector<glm::vec3> colors;

            int current_idx = start_idx;
            bool first = true;

            while (current_idx != -1) {
                const auto& e = ir.elements[current_idx];

                if (e.type != ProceduralElementType::Tube) {
                    current_idx = -1;
                    break;
                }

                // Add tube start position (only for the first tube in the chain)
                if (first) {
                    points.push_back(Vector3(e.position));
                    ups.push_back(Vector3(0, 1, 0));
                    sizes.push_back(e.radius / SPLINE_RADIUS_SCALE);
                    colors.push_back(e.color);
                    first = false;
                }

                // Check if this tube has a ControlPoint child (curve midpoint)
                int cp_child_idx = -1;
                int next_tube_idx = -1;

                if (e.children.size() == 1) {
                    int child_idx = e.children[0];
                    const auto& child = ir.elements[child_idx];

                    if (child.type == ProceduralElementType::ControlPoint) {
                        cp_child_idx = child_idx;
                        // The CP's child (if Tube) continues the branch
                        if (child.children.size() == 1 &&
                            ir.elements[child.children[0]].type == ProceduralElementType::Tube) {
                            next_tube_idx = child.children[0];
                        }
                    } else if (child.type == ProceduralElementType::Tube) {
                        next_tube_idx = child_idx;
                    }
                    // Hub, Puffball, Leaf = end of branch (next_tube_idx stays -1)
                }

                // If there's a control point, insert it BEFORE the tube end position
                // so the spline curves through it: start → CP → end
                if (cp_child_idx != -1) {
                    const auto& cp = ir.elements[cp_child_idx];
                    points.push_back(Vector3(cp.position));
                    ups.push_back(Vector3(0, 1, 0));
                    sizes.push_back(cp.radius / SPLINE_RADIUS_SCALE);
                    colors.push_back(cp.color);
                }

                // Add tube end position
                points.push_back(Vector3(e.end_position));
                ups.push_back(Vector3(0, 1, 0));
                sizes.push_back(e.end_radius / SPLINE_RADIUS_SCALE);
                colors.push_back(e.color);

                current_idx = next_tube_idx;
            }

            if (points.size() >= 2) {
                auto tube_data = Spline::GenerateTube(points, ups, sizes, colors, false, 10, 8);

                unsigned int base = (unsigned int)vertices.size();
                for (const auto& vd : tube_data) {
                    Vertex v;
                    v.Position = vd.pos;
                    v.Normal = vd.normal;
                    v.Color = vd.color;
                    v.TexCoords = glm::vec2(0.5f);
                    vertices.push_back(v);
                }
                // Triangle soup: every 3 consecutive vertices form a triangle
                for (unsigned int i = 0; i < (unsigned int)tube_data.size(); ++i) {
                    indices.push_back(base + i);
                }
            }
        }

        // === PHASE 2: Generate sphere geometry for Hubs and Puffballs ===
        for (const auto& e : ir.elements) {
            if (e.type == ProceduralElementType::Hub) {
                GenerateUVSphere(vertices, indices, e.position, e.radius, e.color, 6, 6);
            } else if (e.type == ProceduralElementType::Puffball) {
                GenerateUVSphere(vertices, indices, e.position, e.radius, e.color, 8, 8);
            }
        }

        // === PHASE 3: Optimize ===
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
