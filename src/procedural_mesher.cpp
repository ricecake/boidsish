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

		void GenerateBox(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			glm::quat                  orientation,
			glm::vec3                  half_extents,
			glm::vec3                  color
		) {
			glm::vec3 corners[8] = {
				{-1, -1, -1},
				{1, -1, -1},
				{1, 1, -1},
				{-1, 1, -1},
				{-1, -1, 1},
				{1, -1, 1},
				{1, 1, 1},
				{-1, 1, 1}
			};
			for (int i = 0; i < 8; ++i)
				corners[i] *= half_extents;

			struct Face {
				glm::vec3 normal;
				int       v[4];
				glm::vec2 uv[4];
			};
			Face faces[6] = {
				{{0, 0, -1}, {0, 3, 2, 1}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}}, // Back
				{{0, 0, 1}, {4, 5, 6, 7}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},  // Front
				{{-1, 0, 0}, {0, 4, 7, 3}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}}, // Left
				{{1, 0, 0}, {1, 2, 6, 5}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}},  // Right
				{{0, -1, 0}, {0, 1, 5, 4}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}}, // Bottom
				{{0, 1, 0}, {3, 7, 6, 2}, {{0, 0}, {0, 1}, {1, 1}, {1, 0}}}   // Top
			};

			for (int i = 0; i < 6; ++i) {
				unsigned int face_base = (unsigned int)vertices.size();
				glm::vec3    normal = orientation * faces[i].normal;
				for (int j = 0; j < 4; ++j) {
					Vertex v;
					v.Position = center + orientation * corners[faces[i].v[j]];
					v.Normal = normal;
					v.Color = color;
					v.TexCoords = faces[i].uv[j];
					vertices.push_back(v);
				}
				indices.push_back(face_base + 0);
				indices.push_back(face_base + 1);
				indices.push_back(face_base + 2);
				indices.push_back(face_base + 0);
				indices.push_back(face_base + 2);
				indices.push_back(face_base + 3);
			}
		}

		void GenerateWedge(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			glm::quat                  orientation,
			glm::vec3                  half_extents,
			glm::vec3                  color
		) {
			glm::vec3 c[6] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}};
			for (int i = 0; i < 6; ++i)
				c[i] *= half_extents;

			auto add_tri = [&](int i1, int i2, int i3, glm::vec3 n) {
				unsigned int b = (unsigned int)vertices.size();
				Vertex       v;
				v.Normal = orientation * n;
				v.Color = color;
				v.Position = center + orientation * c[i1];
				v.TexCoords = {0, 0};
				vertices.push_back(v);
				v.Position = center + orientation * c[i2];
				v.TexCoords = {1, 0};
				vertices.push_back(v);
				v.Position = center + orientation * c[i3];
				v.TexCoords = {0.5, 1};
				vertices.push_back(v);
				indices.push_back(b);
				indices.push_back(b + 1);
				indices.push_back(b + 2);
			};
			auto add_quad = [&](int i1, int i2, int i3, int i4, glm::vec3 n) {
				unsigned int b = (unsigned int)vertices.size();
				Vertex       v;
				v.Normal = orientation * n;
				v.Color = color;
				v.Position = center + orientation * c[i1];
				v.TexCoords = {0, 0};
				vertices.push_back(v);
				v.Position = center + orientation * c[i2];
				v.TexCoords = {1, 0};
				vertices.push_back(v);
				v.Position = center + orientation * c[i3];
				v.TexCoords = {1, 1};
				vertices.push_back(v);
				v.Position = center + orientation * c[i4];
				v.TexCoords = {0, 1};
				vertices.push_back(v);
				indices.push_back(b);
				indices.push_back(b + 1);
				indices.push_back(b + 2);
				indices.push_back(b);
				indices.push_back(b + 2);
				indices.push_back(b + 3);
			};

			add_quad(0, 3, 2, 1, {0, 0, -1}); // Back
			add_quad(0, 1, 5, 4, {0, -1, 0}); // Bottom
			add_quad(4, 5, 2, 3, glm::normalize(glm::vec3(0, half_extents.z, half_extents.y))); // Slope
			add_tri(0, 4, 3, {-1, 0, 0});                                                       // Left
			add_tri(1, 2, 5, {1, 0, 0});                                                        // Right
		}

		void GeneratePyramid(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			glm::quat                  orientation,
			glm::vec3                  half_extents,
			glm::vec3                  color
		) {
			glm::vec3 c[5] = {
				{0, 1, 0},
				{-1, -1, 1},
				{1, -1, 1},
				{1, -1, -1},
				{-1, -1, -1}
			};
			for (int i = 0; i < 5; ++i)
				c[i] *= half_extents;

			auto add_tri = [&](int i1, int i2, int i3) {
				glm::vec3    n = glm::normalize(glm::cross(c[i2] - c[i1], c[i3] - c[i1]));
				unsigned int b = (unsigned int)vertices.size();
				Vertex       v;
				v.Normal = orientation * n;
				v.Color = color;
				v.Position = center + orientation * c[i1];
				v.TexCoords = {0.5, 1};
				vertices.push_back(v);
				v.Position = center + orientation * c[i2];
				v.TexCoords = {0, 0};
				vertices.push_back(v);
				v.Position = center + orientation * c[i3];
				v.TexCoords = {1, 0};
				vertices.push_back(v);
				indices.push_back(b);
				indices.push_back(b + 1);
				indices.push_back(b + 2);
			};

			add_tri(0, 1, 2); // Front
			add_tri(0, 2, 3); // Right
			add_tri(0, 3, 4); // Back
			add_tri(0, 4, 1); // Left

			unsigned int b = (unsigned int)vertices.size();
			Vertex       v;
			v.Normal = orientation * glm::vec3(0, -1, 0);
			v.Color = color;
			v.Position = center + orientation * c[1];
			v.TexCoords = {0, 0};
			vertices.push_back(v);
			v.Position = center + orientation * c[4];
			v.TexCoords = {0, 1};
			vertices.push_back(v);
			v.Position = center + orientation * c[3];
			v.TexCoords = {1, 1};
			vertices.push_back(v);
			v.Position = center + orientation * c[2];
			v.TexCoords = {1, 0};
			vertices.push_back(v);
			indices.push_back(b);
			indices.push_back(b + 1);
			indices.push_back(b + 2);
			indices.push_back(b);
			indices.push_back(b + 2);
			indices.push_back(b + 3);
		}

		void AssignBoneWeights(
			std::vector<Vertex>&            vertices,
			const std::vector<BoneSegment>& bone_segments,
			int                             start_idx = 0,
			int                             end_idx = -1
		) {
			if (bone_segments.empty())
				return;
			if (end_idx == -1)
				end_idx = (int)vertices.size();

			for (int i = start_idx; i < end_idx; ++i) {
				auto& v = vertices[i];
				float best_dist = 1e10f;
				int   best_bone = -1;
				for (int b = 0; b < (int)bone_segments.size(); ++b) {
					const auto& bs = bone_segments[b];
					glm::vec3   ab = bs.end - bs.start;
					glm::vec3   ap = v.Position - bs.start;
					float       denom = glm::dot(ab, ab);
					float       t = (denom > 1e-6f) ? glm::clamp(glm::dot(ap, ab) / denom, 0.0f, 1.0f) : 0.0f;
					float       d = glm::distance(v.Position, bs.start + t * ab);
					if (d < best_dist) {
						best_dist = d;
						best_bone = b;
					}
				}
				if (best_bone != -1) {
					v.m_BoneIDs[0] = best_bone;
					v.m_Weights[0] = 1.0f;
				}
			}
		}

		void AssignRigidWeights(std::vector<Vertex>& vertices, int start_idx, int end_idx, int bone_id) {
			if (bone_id == -1)
				return;
			for (int i = start_idx; i < end_idx; ++i) {
				vertices[i].m_BoneIDs[0] = bone_id;
				vertices[i].m_Weights[0] = 1.0f;
			}
		}

	} // namespace

	std::shared_ptr<Model> ProceduralMesher::GenerateModel(const ProceduralIR& ir) {
		if (ir.elements.empty())
			return nullptr;

		// 1. Bone calculation
		std::vector<BoneSegment> bone_segments;
		std::vector<int>         element_to_bone(ir.elements.size(), -1);
		auto                     data = std::make_shared<ModelData>();
		data->model_path = "procedural_direct_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			bool        should_be_bone = e.is_bone ||
				(ir.name == "critter" && e.type == ProceduralElementType::Tube && e.length > 0.1f);

			if (should_be_bone) {
				BoneSegment bs;
				if (e.type == ProceduralElementType::Tube) {
					bs.start = e.position;
					bs.end = e.end_position;
				} else {
					bs.start = e.position;
					bs.end = e.position + e.orientation * glm::vec3(0, 0.1f, 0);
				}
				bs.parent_bone = (e.parent != -1) ? element_to_bone[e.parent] : -1;

				glm::vec3 dir = bs.end - bs.start;
				float     len = glm::length(dir);
				if (len < 0.001f)
					dir = e.orientation * glm::vec3(0, 1, 0);
				else
					dir = glm::normalize(dir);

				glm::vec3 up = (std::abs(dir.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
				glm::vec3 right = glm::normalize(glm::cross(up, dir));
				up = glm::normalize(glm::cross(dir, right));

				glm::mat4 bone_to_model(1.0f);
				bone_to_model[0] = glm::vec4(right, 0);
				bone_to_model[1] = glm::vec4(dir, 0);
				bone_to_model[2] = glm::vec4(up, 0);
				bone_to_model[3] = glm::vec4(bs.start, 1);

				bs.offset = glm::inverse(bone_to_model);

				int bone_id = (int)bone_segments.size();
				bone_segments.push_back(bs);
				element_to_bone[i] = bone_id;

				std::string bname = e.name.empty() ? ("bone_" + std::to_string(bone_id)) : e.name;
				std::string pname = "";
				if (e.parent != -1) {
					int p_elem = e.parent;
					while (p_elem != -1) {
						if (ir.elements[p_elem].is_bone || (ir.name == "critter" &&
						                                    ir.elements[p_elem].type == ProceduralElementType::Tube &&
						                                    ir.elements[p_elem].length > 0.1f)) {
							pname = ir.elements[p_elem].name.empty() ? ("bone_" + std::to_string(element_to_bone[p_elem]))
																	 : ir.elements[p_elem].name;
							break;
						}
						p_elem = ir.elements[p_elem].parent;
					}
				}

				glm::mat4 local = bs.offset; // This is actually inv(global) right now
				if (bs.parent_bone != -1) {
					local = bone_segments[bs.parent_bone].offset * glm::inverse(bs.offset);
				} else {
					local = glm::inverse(bs.offset);
				}

				data->AddBone(bname, pname, local);
				element_to_bone[i] = bone_id;
			} else if (e.parent != -1) {
				element_to_bone[i] = element_to_bone[e.parent];
			}
		}

		// 2. Generate Primary Mesh
		std::vector<Vertex>       main_vertices;
		std::vector<unsigned int> main_indices;

		struct SkinJob {
			int start_v;
			int end_v;
			int element_idx;
			int mode; // Bits 0-7: SkinningMode, Bit 31: IsLeaf
		};
		std::vector<SkinJob> skin_jobs;

		// Tubes handled via spline if multiple connected
		std::vector<bool> handled(ir.elements.size(), false);
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			if (e.type != ProceduralElementType::Tube || handled[i])
				continue;

			// Spline tube branch detection
			if (e.parent == -1 || ir.elements[e.parent].type != ProceduralElementType::Tube) {
				std::vector<Vector3>   points;
				std::vector<Vector3>   ups;
				std::vector<float>     sizes;
				std::vector<glm::vec3> colors;

				int  curr = i;
				bool first = true;
				while (curr != -1) {
					const auto& te = ir.elements[curr];
					if (te.type != ProceduralElementType::Tube || handled[curr])
						break;
					handled[curr] = true;

					if (first) {
						points.push_back(Vector3(te.position));
						ups.push_back(Vector3(0, 1, 0));
						sizes.push_back(te.radius / SPLINE_RADIUS_SCALE);
						colors.push_back(te.color);
						first = false;
					}
					// Find next tube or control point
					int next = -1;
					for (int child : te.children) {
						if (ir.elements[child].type == ProceduralElementType::Tube) {
							next = child;
							break;
						}
						if (ir.elements[child].type == ProceduralElementType::ControlPoint) {
							const auto& cp = ir.elements[child];
							points.push_back(Vector3(cp.position));
							ups.push_back(Vector3(0, 1, 0));
							sizes.push_back(cp.radius / SPLINE_RADIUS_SCALE);
							colors.push_back(cp.color);
							for (int cp_child : cp.children) {
								if (ir.elements[cp_child].type == ProceduralElementType::Tube) {
									next = cp_child;
									break;
								}
							}
							break;
						}
					}
					points.push_back(Vector3(te.end_position));
					ups.push_back(Vector3(0, 1, 0));
					sizes.push_back(te.end_radius / SPLINE_RADIUS_SCALE);
					colors.push_back(te.color);
					curr = next;
				}

				if (points.size() >= 2) {
					int          v_start = (int)main_vertices.size();
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
					for (unsigned int k = 0; k < (unsigned int)tube_data.size(); ++k)
						main_indices.push_back(base + k);

					skin_jobs.push_back({v_start, (int)main_vertices.size(), i, (int)SkinningMode::Smooth});
				}
			}
		}

		// Hubs, Boxes, Wedges, Pyramids, Puffballs
		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			if (handled[i]) continue;
			int         v_start = (int)main_vertices.size();

			if (e.type == ProceduralElementType::Hub) {
				GenerateUVSphere(main_vertices, main_indices, e.position, e.radius, e.color, 6, 6);
			} else if (e.type == ProceduralElementType::Box) {
				GenerateBox(main_vertices, main_indices, e.position, e.orientation, e.dimensions, e.color);
			} else if (e.type == ProceduralElementType::Wedge) {
				GenerateWedge(main_vertices, main_indices, e.position, e.orientation, e.dimensions, e.color);
			} else if (e.type == ProceduralElementType::Pyramid) {
				GeneratePyramid(main_vertices, main_indices, e.position, e.orientation, e.dimensions, e.color);
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
			} else {
				continue;
			}

			SkinningMode sm = e.skinning_mode;
			if (sm == SkinningMode::Auto) {
				if (e.type == ProceduralElementType::Hub || e.type == ProceduralElementType::Puffball)
					sm = SkinningMode::Smooth;
				else
					sm = SkinningMode::Rigid;
			}
			skin_jobs.push_back({v_start, (int)main_vertices.size(), i, (int)sm});
		}

		// 3. Generate Leaves
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

		for (int i = 0; i < (int)ir.elements.size(); ++i) {
			const auto& e = ir.elements[i];
			if (e.type == ProceduralElementType::Leaf) {
				int v_start = (int)leaf_vertices.size();
				AddLeafGeom(e.position, e.orientation, e.radius, e.color, e.variant);

				SkinningMode sm = e.skinning_mode;
				if (sm == SkinningMode::Auto)
					sm = SkinningMode::Rigid;
				skin_jobs.push_back({v_start, (int)leaf_vertices.size(), i, (int)sm | (int)0x80000000}); // Mark as leaf
			}
		}

		// 4. Weighting, Repositioning, and AABB
		for (auto& job : skin_jobs) {
			bool                 is_leaf = (job.mode & 0x80000000) != 0;
			SkinningMode         mode = (SkinningMode)(job.mode & 0x7FFFFFFF);
			std::vector<Vertex>& verts = is_leaf ? leaf_vertices : main_vertices;

			if (mode == SkinningMode::Rigid) {
				int bone_id = element_to_bone[job.element_idx];
				AssignRigidWeights(verts, job.start_v, job.end_v, bone_id);
			} else if (mode == SkinningMode::Smooth) {
				AssignBoneWeights(verts, bone_segments, job.start_v, job.end_v);
			}
		}

		// Calculate AABB and reposition
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

		if (has_any) {
			if (ir.name == "critter") {
				float y_offset = -min.y;
				for (auto& v : main_vertices)
					v.Position.y += y_offset;
				for (auto& v : leaf_vertices)
					v.Position.y += y_offset;
				max.y += y_offset;
				min.y = 0;
				for (auto& pair : data->bone_info_map)
					pair.second.offset = pair.second.offset *
						glm::translate(glm::mat4(1.0f), glm::vec3(0, -y_offset, 0));
			}
			data->aabb = AABB(min, max);
		}

		// 5. Finalize Meshes
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
