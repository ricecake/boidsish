#include "procedural_mesher.h"

#include <numbers>

#include "ConfigManager.h"
#include "mesh_optimizer_util.h"
#include "spline.h"

namespace Boidsish {

	namespace {
		const float SPLINE_RADIUS_SCALE = 0.005f;

		struct BoneSegment {
			int       parent_bone;
			glm::mat4 offset;
			glm::vec3 start;
			glm::vec3 end;
			bool      is_hub = false;
		};

		void GenerateUVSphere(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			float                      radius,
			glm::vec3                  color,
			int                        lat_segments,
			int                        lon_segments,
			glm::vec3                  scale = glm::vec3(1.0f)
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
					v.Position = center + normal * radius * scale;
					v.Normal = glm::normalize(normal / scale); // Adjusted normal for non-uniform scale
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

		void AssignBoneWeights(std::vector<Vertex>& vertices, const std::vector<BoneSegment>& bone_segments) {
			if (bone_segments.empty())
				return;

			for (auto& v : vertices) {
				struct Candidate {
					int   id;
					float dist;
				};
				std::vector<Candidate> candidates;

				for (int b = 0; b < (int)bone_segments.size(); ++b) {
					const auto& bs = bone_segments[b];
					float       d = 0;
					if (bs.is_hub || glm::distance(bs.start, bs.end) < 0.001f) {
						d = glm::distance(v.Position, bs.start);
					} else {
						glm::vec3 ab = bs.end - bs.start;
						glm::vec3 ap = v.Position - bs.start;
						float     t = glm::clamp(glm::dot(ap, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
						d = glm::distance(v.Position, bs.start + t * ab);
					}
					candidates.push_back({b, d});
				}

				std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
					return a.dist < b.dist;
				});

				float total_inv_dist = 0;
				int   count = 0;
				for (int i = 0; i < 4 && i < (int)candidates.size(); ++i) {
					if (candidates[i].dist < 2.0f || i == 0) {
						total_inv_dist += 1.0f / (candidates[i].dist + 0.001f);
						count++;
					}
				}

				for (int i = 0; i < count; ++i) {
					v.m_BoneIDs[i] = candidates[i].id;
					v.m_Weights[i] = (1.0f / (candidates[i].dist + 0.001f)) / total_inv_dist;
				}
				for (int i = count; i < 4; ++i) {
					v.m_BoneIDs[i] = -1;
					v.m_Weights[i] = 0.0f;
				}
			}
		}

	} // namespace

	std::shared_ptr<Model> ProceduralMesher::GenerateModel(const ProceduralIR& ir) {
		if (ir.elements.empty())
			return nullptr;

		auto data = std::make_shared<ModelData>();
		data->model_path = "procedural_direct_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

		// 1. Generate Primary Mesh Geometry (Tubes, Hubs, Puffballs)
		std::vector<Vertex>       main_vertices;
		std::vector<unsigned int> main_indices;

		// Generate tubes
		std::vector<int> branch_starts;
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			if (e.type != ProceduralElementType::Tube)
				continue;
			if (e.parent == -1 || ir.elements[e.parent].type == ProceduralElementType::Hub) {
				branch_starts.push_back(i);
			}
		}

		for (int start_idx : branch_starts) {
			std::vector<Vector3>   points;
			std::vector<Vector3>   ups;
			std::vector<float>     sizes;
			std::vector<glm::vec3> colors;
			int                    current_idx = start_idx;
			bool                   first = true;
			while (current_idx != -1) {
				const auto& e = ir.elements[current_idx];
				if (e.type != ProceduralElementType::Tube)
					break;
				if (first) {
					points.push_back(Vector3(e.position));
					ups.push_back(Vector3(0, 1, 0));
					sizes.push_back(e.radius / SPLINE_RADIUS_SCALE);
					colors.push_back(e.color);
					first = false;
				}
				int cp_child_idx = -1;
				int next_tube_idx = -1;
				if (e.children.size() == 1) {
					int child_idx = e.children[0];
					if (ir.elements[child_idx].type == ProceduralElementType::ControlPoint) {
						cp_child_idx = child_idx;
						if (ir.elements[child_idx].children.size() == 1 &&
						    ir.elements[ir.elements[child_idx].children[0]].type == ProceduralElementType::Tube) {
							next_tube_idx = ir.elements[child_idx].children[0];
						}
					} else if (ir.elements[child_idx].type == ProceduralElementType::Tube) {
						next_tube_idx = child_idx;
					}
				}
				if (cp_child_idx != -1) {
					const auto& cp = ir.elements[cp_child_idx];
					points.push_back(Vector3(cp.position));
					ups.push_back(Vector3(0, 1, 0));
					sizes.push_back(cp.radius / SPLINE_RADIUS_SCALE);
					colors.push_back(cp.color);
				}
				points.push_back(Vector3(e.end_position));
				ups.push_back(Vector3(0, 1, 0));
				sizes.push_back(e.end_radius / SPLINE_RADIUS_SCALE);
				colors.push_back(e.color);
				current_idx = next_tube_idx;
			}
			if (points.size() >= 2) {
				auto         tube_data = Spline::GenerateTube(points, ups, sizes, colors, false, 10, 8);
				unsigned int base = (unsigned int)main_vertices.size();
				for (const auto& vd : tube_data) {
					Vertex v;
					v.Position = vd.pos;
					v.Normal = vd.normal;
					v.Color = vd.color;
					v.TexCoords = glm::vec2(0.5f);
					main_vertices.push_back(v);
				}
				for (unsigned int i = 0; i < (unsigned int)tube_data.size(); ++i)
					main_indices.push_back(base + i);
			}
		}

		// Generate Hubs and Puffballs
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			if (e.type == ProceduralElementType::Hub) {
				GenerateUVSphere(main_vertices, main_indices, e.position, e.radius, e.color, 6, 6);
			} else if (e.type == ProceduralElementType::Puffball) {
				if (e.variant == 1)
					GenerateUVSphere(
						main_vertices,
						main_indices,
						e.position,
						e.radius,
						e.color,
						8,
						8,
						glm::vec3(1.0f, 0.4f, 1.0f)
					);
				else
					GenerateUVSphere(main_vertices, main_indices, e.position, e.radius, e.color, 4, 4);
			}
		}

		// Generate Leaves
		std::vector<Vertex>       leaf_vertices;
		std::vector<unsigned int> leaf_indices;
		auto AddLeafGeom = [&](glm::vec3 pos, glm::quat ori, float size, glm::vec3 color, int variant) {
			unsigned int           base = (unsigned int)leaf_vertices.size();
			std::vector<glm::vec3> pts;
			if (variant == 1)
				pts = {{0, 0, 0}, {0.1f, 0.5f, 0}, {0, 1.2f, 0}, {-0.1f, 0.5f, 0}};
			else if (variant == 2)
				pts = {
					{0, 0, 0},
					{0.4f, 0.2f, 0},
					{0.2f, 0.4f, 0},
					{0.5f, 0.6f, 0},
					{0, 1.0f, 0},
					{-0.5f, 0.6f, 0},
					{-0.2f, 0.4f, 0},
					{-0.4f, 0.2f, 0}
				};
			else if (variant == 3)
				pts = {{0, 0, 0}, {0.4f, 0.4f, 0}, {0.2f, 0.8f, 0}, {0, 0.7f, 0}, {-0.2f, 0.8f, 0}, {-0.4f, 0.4f, 0}};
			else
				pts = {{0, 0, 0}, {0.3f, 0.5f, 0.1f}, {0, 1.0f, 0}, {-0.3f, 0.5f, -0.1f}};

			for (auto& p : pts) {
				p = pos + ori * (p * size);
				Vertex v;
				v.Position = p;
				v.Normal = ori * glm::vec3(0, 0, 1);
				v.Color = color;
				v.TexCoords = glm::vec2(0.5f);
				leaf_vertices.push_back(v);
			}
			for (size_t i = 1; i < pts.size() - 1; ++i) {
				leaf_indices.push_back(base);
				leaf_indices.push_back(base + (unsigned int)i);
				leaf_indices.push_back(base + (unsigned int)i + 1);
				leaf_indices.push_back(base);
				leaf_indices.push_back(base + (unsigned int)i + 1);
				leaf_indices.push_back(base + (unsigned int)i);
			}
		};

		for (const auto& e : ir.elements) {
			if (e.type == ProceduralElementType::Leaf)
				AddLeafGeom(e.position, e.orientation, e.radius, e.color, e.variant);
		}

		// Calculate AABB and repositioning offset
		glm::vec3 min(1e10f), max(-1e10f);
		bool      has_any = false;
		auto      update_aabb = [&](const std::vector<Vertex>& verts) {
            for (const auto& v : verts) {
                min = glm::min(min, v.Position);
                max = glm::max(max, v.Position);
                has_any = true;
            }
		};
		update_aabb(main_vertices);
		update_aabb(leaf_vertices);

		float y_offset = 0;
		if (has_any && ir.name == "critter") {
			y_offset = -min.y;
			for (auto& v : main_vertices)
				v.Position.y += y_offset;
			for (auto& v : leaf_vertices)
				v.Position.y += y_offset;
			max.y += y_offset;
			min.y = 0;
			data->aabb = AABB(min, max);
		} else if (has_any) {
			data->aabb = AABB(min, max);
		}

		// 2. Create bones and weights if it's a critter
		if (ir.name == "critter") {
			std::vector<BoneSegment> bone_segments;
			std::vector<int>         element_to_bone(ir.elements.size(), -1);
			std::vector<glm::mat4>   bone_global_transforms;

			for (int i = 0; i < (int)ir.elements.size(); ++i) {
				const auto& e = ir.elements[i];
				if ((e.type == ProceduralElementType::Tube && e.length > 0.1f) || e.type == ProceduralElementType::Hub) {
					BoneSegment bs;
					bs.start = e.position;
					bs.start.y += y_offset;
					bs.parent_bone = (e.parent != -1) ? element_to_bone[e.parent] : -1;
					bs.is_hub = (e.type == ProceduralElementType::Hub);

					glm::mat4 bone_to_model(1.0f);
					if (e.type == ProceduralElementType::Tube) {
						bs.end = e.end_position;
						bs.end.y += y_offset;
						glm::vec3 dir = glm::normalize(bs.end - bs.start);
						glm::vec3 up = (std::abs(dir.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
						glm::vec3 right = glm::normalize(glm::cross(up, dir));
						up = glm::normalize(glm::cross(dir, right));

						bone_to_model[0] = glm::vec4(right, 0);
						bone_to_model[1] = glm::vec4(dir, 0);
						bone_to_model[2] = glm::vec4(up, 0);
						bone_to_model[3] = glm::vec4(bs.start, 1);
					} else {
						bs.end = bs.start;
						bone_to_model = glm::translate(glm::mat4(1.0f), bs.start);
					}

					int bone_id = (int)bone_segments.size();
					bone_segments.push_back(bs);
					bone_global_transforms.push_back(bone_to_model);
					element_to_bone[i] = bone_id;

					std::string bone_name = e.name.empty() ? ("bone_" + std::to_string(bone_id)) : e.name;
					int         parent_bone_idx = bs.parent_bone;
					std::string parent_name = (parent_bone_idx != -1)
						? (ir.elements[ir.elements[i].parent].name.empty()
					           ? ("bone_" + std::to_string(parent_bone_idx))
					           : ir.elements[ir.elements[i].parent].name)
						: "";

					glm::mat4 local_transform = bone_to_model;
					if (parent_bone_idx != -1) {
						local_transform = glm::inverse(bone_global_transforms[parent_bone_idx]) * bone_to_model;
					}

					data->AddBone(bone_name, parent_name, local_transform);
				} else if (e.parent != -1) {
					element_to_bone[i] = element_to_bone[e.parent];
				}
			}
			AssignBoneWeights(main_vertices, bone_segments);
			AssignBoneWeights(leaf_vertices, bone_segments);
		}

		// 3. Finalize Meshes
		auto finalize_mesh =
			[&](std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, bool use_simplifier) {
				if (vertices.empty())
					return;
				auto& config = ConfigManager::GetInstance();
				if (use_simplifier && config.GetAppSettingBool("mesh_simplifier_enabled", false))
					MeshOptimizerUtil::Simplify(vertices, indices, 0.05f, 0.5f, 40, data->model_path);

				std::vector<unsigned int> shadow_indices;
				if (config.GetAppSettingBool("mesh_optimizer_enabled", true)) {
					MeshOptimizerUtil::Optimize(vertices, indices, data->model_path);
					if (config.GetAppSettingBool("mesh_optimizer_shadow_indices_enabled", true))
						MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);
				}
				Mesh m(vertices, indices, {}, shadow_indices);
				m.diffuseColor = {1, 1, 1};
				m.has_vertex_colors = true;
				data->meshes.push_back(m);
			};

		finalize_mesh(main_vertices, main_indices, true);
		finalize_mesh(leaf_vertices, leaf_indices, false);

		return std::make_shared<Model>(data, false);
	}

	std::shared_ptr<ModelData> ProceduralMesher::GenerateDirectMesh(const ProceduralIR& ir) {
		// Redundant with GenerateModel but kept for interface compatibility if needed, though GenerateModel now does
		// the heavy lifting
		return nullptr;
	}

} // namespace Boidsish
