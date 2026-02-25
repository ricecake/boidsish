#include "procedural_generator.h"

#include <numbers>
#include <random>

#include "spline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	namespace {
		// SDF Primitive Functions
		float sdfSphere(glm::vec3 p, glm::vec3 center, float radius) {
			return glm::length(p - center) - radius;
		}

		float sdfCapsule(glm::vec3 p, glm::vec3 a, glm::vec3 b, float r) {
			glm::vec3 pa = p - a, ba = b - a;
			float     h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
			return glm::length(pa - ba * h) - r;
		}

		float smoothUnion(float d1, float d2, float k) {
			float h = glm::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
			return glm::mix(d2, d1, h) - k * h * (1.0f - h);
		}

		struct SDFPrimitive {
			enum Type { Sphere, Capsule };
			Type      type;
			glm::vec3 a, b;
			float     radius;
			glm::vec3 color;
		};

		std::shared_ptr<ModelData> VoxelizeSurfaceNet(const std::vector<SDFPrimitive>& primitives, const glm::vec3& diffuseColor) {
			if (primitives.empty())
				return std::make_shared<ModelData>();

			// 1. Calculate AABB
			glm::vec3 minP = primitives[0].a - glm::vec3(primitives[0].radius + 0.5f);
			glm::vec3 maxP = primitives[0].a + glm::vec3(primitives[0].radius + 0.5f);
			for (const auto& p : primitives) {
				minP = glm::min(minP, p.a - glm::vec3(p.radius + 0.5f));
				maxP = glm::max(maxP, p.a + glm::vec3(p.radius + 0.5f));
				if (p.type == SDFPrimitive::Capsule) {
					minP = glm::min(minP, p.b - glm::vec3(p.radius + 0.5f));
					maxP = glm::max(maxP, p.b + glm::vec3(p.radius + 0.5f));
				}
			}

			// 2. Define grid
			const int res = 24;
			glm::vec3 size = maxP - minP;
			glm::vec3 step = size / (float)(res - 1);

			auto sampleSDF = [&](glm::vec3 p) -> std::pair<float, glm::vec3> {
				float     d_all = 1e10f;
				float     minDist = 1e10f;
				glm::vec3 resCol = glm::vec3(1.0f);
				for (const auto& prim : primitives) {
					float d;
					if (prim.type == SDFPrimitive::Sphere)
						d = sdfSphere(p, prim.a, prim.radius);
					else
						d = sdfCapsule(p, prim.a, prim.b, prim.radius);

					if (d < minDist) {
						minDist = d;
						resCol = prim.color;
					}
					d_all = smoothUnion(d_all, d, 0.05f);
				}
				return {d_all, resCol};
			};

			std::vector<float>     values(res * res * res);
			std::vector<glm::vec3> colors(res * res * res);

			for (int z = 0; z < res; ++z) {
				for (int y = 0; y < res; ++y) {
					for (int x = 0; x < res; ++x) {
						glm::vec3 p = minP + glm::vec3(x, y, z) * step;
						auto      res_sdf = sampleSDF(p);
						values[z * res * res + y * res + x] = res_sdf.first;
						colors[z * res * res + y * res + x] = res_sdf.second;
					}
				}
			}

			// 3. Surface Net extraction
			std::vector<Vertex>       vertices;
			std::vector<unsigned int> indices;
			std::vector<int>          grid_indices(res * res * res, -1);

			// For each cell, create a vertex if it's crossed
			for (int z = 0; z < res - 1; ++z) {
				for (int y = 0; y < res - 1; ++y) {
					for (int x = 0; x < res - 1; ++x) {
						int corners[8] = {
							(z + 0) * res * res + (y + 0) * res + (x + 0), (z + 0) * res * res + (y + 0) * res + (x + 1),
							(z + 0) * res * res + (y + 1) * res + (x + 0), (z + 0) * res * res + (y + 1) * res + (x + 1),
							(z + 1) * res * res + (y + 0) * res + (x + 0), (z + 1) * res * res + (y + 0) * res + (x + 1),
							(z + 1) * res * res + (y + 1) * res + (x + 0), (z + 1) * res * res + (y + 1) * res + (x + 1)
						};

						bool inside = values[corners[0]] < 0.0f;
						bool boundary = false;
						for (int i = 1; i < 8; ++i) {
							if ((values[corners[i]] < 0.0f) != inside) {
								boundary = true;
								break;
							}
						}

						if (boundary) {
							grid_indices[z * res * res + y * res + x] = static_cast<int>(vertices.size());
							Vertex v;
							v.Position = minP + (glm::vec3(x, y, z) + 0.5f) * step;
							v.Normal = glm::vec3(0, 1, 0);
							v.Color = colors[z * res * res + y * res + x];
							v.TexCoords = glm::vec2(0.5f);
							vertices.push_back(v);
						}
					}
				}
			}

			// For each edge, create quads
			for (int z = 0; z < res - 1; ++z) {
				for (int y = 0; y < res - 1; ++y) {
					for (int x = 0; x < res - 1; ++x) {
						int curr = z * res * res + y * res + x;

						// X-edge (between (x,y,z) and (x+1,y,z))
						if (x < res - 2 && y > 0 && z > 0) {
							bool inside0 = values[curr] < 0.0f;
							bool inside1 = values[curr + 1] < 0.0f;
							if (inside0 != inside1) {
								int v0 = grid_indices[curr];
								int v1 = grid_indices[curr - res];
								int v2 = grid_indices[curr - res * res - res];
								int v3 = grid_indices[curr - res * res];
								if (v0 != -1 && v1 != -1 && v2 != -1 && v3 != -1) {
									if (inside0) {
										indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v3);
									} else {
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v1);
										indices.push_back(v0); indices.push_back(v3); indices.push_back(v2);
									}
								}
							}
						}

						// Y-edge
						if (y < res - 2 && x > 0 && z > 0) {
							bool inside0 = values[curr] < 0.0f;
							bool inside1 = values[curr + res] < 0.0f;
							if (inside0 != inside1) {
								int v0 = grid_indices[curr];
								int v1 = grid_indices[curr - 1];
								int v2 = grid_indices[curr - res * res - 1];
								int v3 = grid_indices[curr - res * res];
								if (v0 != -1 && v1 != -1 && v2 != -1 && v3 != -1) {
									if (inside0) {
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v1);
										indices.push_back(v0); indices.push_back(v3); indices.push_back(v2);
									} else {
										indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v3);
									}
								}
							}
						}

						// Z-edge
						if (z < res - 2 && x > 0 && y > 0) {
							bool inside0 = values[curr] < 0.0f;
							bool inside1 = values[curr + res * res] < 0.0f;
							if (inside0 != inside1) {
								int v0 = grid_indices[curr];
								int v1 = grid_indices[curr - 1];
								int v2 = grid_indices[curr - res - 1];
								int v3 = grid_indices[curr - res];
								if (v0 != -1 && v1 != -1 && v2 != -1 && v3 != -1) {
									if (inside0) {
										indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v3);
									} else {
										indices.push_back(v0); indices.push_back(v2); indices.push_back(v1);
										indices.push_back(v0); indices.push_back(v3); indices.push_back(v2);
									}
								}
							}
						}
					}
				}
			}

			// Calculate Normals
			for (size_t i = 0; i < indices.size(); i += 3) {
				glm::vec3 v1 = vertices[indices[i]].Position;
				glm::vec3 v2 = vertices[indices[i + 1]].Position;
				glm::vec3 v3 = vertices[indices[i + 2]].Position;
				glm::vec3 n = glm::normalize(glm::cross(v2 - v1, v3 - v1));
				vertices[indices[i]].Normal = n;
				vertices[indices[i + 1]].Normal = n;
				vertices[indices[i + 2]].Normal = n;
			}

			auto data = std::make_shared<ModelData>();
			Mesh mesh(vertices, indices, {});
			mesh.diffuseColor = diffuseColor;
			data->meshes.push_back(mesh);
			data->aabb = AABB(minP, maxP);
			data->model_path = "voxelized_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));
			return data;
		}

		struct LSystem {
			std::string                 axiom;
			std::map<char, std::string> rules;

			std::string expand(int iterations) {
				std::string current = axiom;
				for (int i = 0; i < iterations; ++i) {
					std::string next;
					for (char c : current) {
						if (rules.count(c))
							next += rules.at(c);
						else
							next += c;
					}
					current = next;
				}
				return current;
			}
		};

		std::map<char, std::string> ParseRules(const std::vector<std::string>& ruleStrings) {
			std::map<char, std::string> rules;
			for (const auto& rule : ruleStrings) {
				size_t pos = rule.find('=');
				if (pos != std::string::npos && pos > 0) {
					rules[rule[0]] = rule.substr(pos + 1);
				}
			}
			return rules;
		}

		void AddPuffball(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			float                      radius,
			glm::vec3                  color,
			std::mt19937&              gen,
			glm::vec3                  scale = glm::vec3(1.0f)
		) {
			std::uniform_real_distribution<float> dis(-0.05f * radius, 0.05f * radius);
			unsigned int                          base = vertices.size();
			const int                             lat_segments = 4;
			const int                             lon_segments = 4;

			for (int lat = 0; lat <= lat_segments; ++lat) {
				float theta = lat * (float)std::numbers::pi / lat_segments;
				float sinTheta = std::sin(theta);
				float cosTheta = std::cos(theta);
				for (int lon = 0; lon <= lon_segments; ++lon) {
					float phi = lon * 2.0f * (float)std::numbers::pi / lon_segments;
					float sinPhi = std::sin(phi);
					float cosPhi = std::cos(phi);

					glm::vec3 normal(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
					glm::vec3 jitter(dis(gen), dis(gen), dis(gen));

					Vertex v;
					v.Position = center + (normal * radius * scale) + jitter;
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

		void AddLeaf(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  pos,
			glm::quat                  ori,
			float                      size,
			glm::vec3                  color,
			bool                       arrowhead = false
		) {
			unsigned int base = vertices.size();
			unsigned int start_indices = static_cast<unsigned int>(indices.size());

			std::vector<glm::vec3> pts;
			if (arrowhead) {
				// Arrowhead shape with a slight notch
				pts = {{0, 0, 0}, {0.5f, 0.1f, 0}, {0, 1.0f, 0}, {-0.5f, 0.1f, 0}, {0, 0.2f, 0}};
			} else {
				// More rounded leaf shape (diamond with midpoints)
				pts = {{0, 0, 0}, {0.3f, 0.5f, 0.1f}, {0, 1.0f, 0}, {-0.3f, 0.5f, -0.1f}};
			}

			for (auto& p : pts) {
				p = pos + ori * (p * size);
				Vertex v;
				v.Position = p;
				v.Normal = ori * glm::vec3(0, 0, 1);
				v.Color = color;
				v.TexCoords = glm::vec2(0.5f);
				vertices.push_back(v);
			}

			if (arrowhead) {
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 4);

				indices.push_back(base + 1);
				indices.push_back(base + 2);
				indices.push_back(base + 4);

				indices.push_back(base + 2);
				indices.push_back(base + 3);
				indices.push_back(base + 4);

				indices.push_back(base + 3);
				indices.push_back(base + 0);
				indices.push_back(base + 4);

				// Double sided (only for current triangles)
				size_t cur_indices = indices.size();
				for (size_t i = start_indices; i < cur_indices; i += 3) {
					indices.push_back(indices[i + 2]);
					indices.push_back(indices[i + 1]);
					indices.push_back(indices[i]);
				}
			} else {
				indices.push_back(base);
				indices.push_back(base + 1);
				indices.push_back(base + 2);
				indices.push_back(base);
				indices.push_back(base + 2);
				indices.push_back(base + 3);

				// Double sided
				indices.push_back(base);
				indices.push_back(base + 2);
				indices.push_back(base + 1);
				indices.push_back(base);
				indices.push_back(base + 3);
				indices.push_back(base + 2);
			}
		}

		void AddSplineTube(
			std::vector<Vertex>&          vertices,
			std::vector<unsigned int>&    indices,
			const std::vector<glm::vec3>& pts,
			const std::vector<float>&     radii,
			const std::vector<glm::vec3>& colors,
			int                           curve_segments = 10,
			int                           cylinder_segments = 12
		) {
			if (pts.size() < 2)
				return;

			std::vector<Vector3>   s_pts;
			std::vector<Vector3>   s_ups;
			std::vector<float>     s_sizes;
			std::vector<glm::vec3> s_colors;

			for (size_t i = 0; i < pts.size(); ++i) {
				s_pts.push_back(Vector3(pts[i]));
				s_ups.push_back(Vector3(0, 1, 0));    // Simple up
				s_sizes.push_back(radii[i] / 0.005f); // Compensate for internal scale
				s_colors.push_back(colors[i]);
			}

			auto v_data = Spline::GenerateTube(s_pts, s_ups, s_sizes, s_colors, false, curve_segments, cylinder_segments);

			unsigned int base = vertices.size();
			for (const auto& vd : v_data) {
				Vertex v;
				v.Position = vd.pos;
				v.Normal = vd.normal;
				v.Color = vd.color;
				v.TexCoords = glm::vec2(0.5f);
				vertices.push_back(v);
			}

			for (unsigned int i = 0; i < v_data.size(); ++i) {
				indices.push_back(base + i);
			}
		}

		void WeldVertices(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
			if (vertices.empty() || indices.empty())
				return;

			std::vector<Vertex>            new_vertices;
			std::vector<unsigned int>      new_indices;
			std::map<size_t, unsigned int> vertex_map;

			auto hash_vertex = [](const Vertex& v) {
				size_t h1 = std::hash<float>{}(std::round(v.Position.x * 1000.0f));
				size_t h2 = std::hash<float>{}(std::round(v.Position.y * 1000.0f));
				size_t h3 = std::hash<float>{}(std::round(v.Position.z * 1000.0f));
				return h1 ^ (h2 << 1) ^ (h3 << 2);
			};

			new_indices.reserve(indices.size());

			for (unsigned int idx : indices) {
				const Vertex& v = vertices[idx];
				size_t        h = hash_vertex(v);

				if (vertex_map.find(h) == vertex_map.end()) {
					vertex_map[h] = static_cast<unsigned int>(new_vertices.size());
					new_vertices.push_back(v);
				}
				new_indices.push_back(vertex_map[h]);
			}

			vertices = std::move(new_vertices);
			indices = std::move(new_indices);
		}
	} // namespace

	std::shared_ptr<Model> ProceduralGenerator::Generate(ProceduralType type, unsigned int seed) {
		switch (type) {
		case ProceduralType::Rock:
			return GenerateRock(seed);
		case ProceduralType::Grass:
			return GenerateGrass(seed);
		case ProceduralType::Flower:
			return GenerateFlower(seed);
		case ProceduralType::Tree:
			return GenerateTree(seed);
		}
		return nullptr;
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateRock(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.15f, 0.15f);
		std::uniform_real_distribution<float> colDis(-0.1f, 0.1f);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		// Icosahedron vertices
		const float            t = (1.0f + std::sqrt(5.0f)) / 2.0f;
		std::vector<glm::vec3> pts = {
			{-1, t, 0},
			{1, t, 0},
			{-1, -t, 0},
			{1, -t, 0},
			{0, -1, t},
			{0, 1, t},
			{0, -1, -t},
			{0, 1, -t},
			{t, 0, -1},
			{t, 0, 1},
			{-t, 0, -1},
			{-t, 0, 1}
		};

		glm::vec3 baseCol(0.4f + colDis(gen), 0.4f + colDis(gen), 0.35f + colDis(gen));

		for (auto& p : pts) {
			p = glm::normalize(p);
			p += glm::vec3(dis(gen), dis(gen), dis(gen));

			Vertex v;
			v.Position = p;
			v.Normal = glm::normalize(p);
			v.Color = baseCol + glm::vec3(colDis(gen), colDis(gen), colDis(gen));
			vertices.push_back(v);
		}

		indices = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
		           3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1};

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)));
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateGrass(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		const int num_blades = 5;
		for (int i = 0; i < num_blades; ++i) {
			float angle = (float)i / num_blades * 2.0f * (float)std::numbers::pi;
			float s = std::sin(angle);
			float c = std::cos(angle);

			std::vector<glm::vec3> pts;
			std::vector<float>     radii;
			std::vector<glm::vec3> colors;

			glm::vec3 basePos(c * 0.1f, 0, s * 0.1f);
			glm::vec3 grassCol(0.1f, 0.5f + dis(gen), 0.1f);

			pts.push_back(basePos);
			radii.push_back(0.05f);
			colors.push_back(grassCol);

			pts.push_back(basePos + glm::vec3(dis(gen), 0.5f, dis(gen)));
			radii.push_back(0.03f);
			colors.push_back(grassCol * 1.2f);

			pts.push_back(basePos + glm::vec3(dis(gen) * 2.0f, 1.0f, dis(gen) * 2.0f));
			radii.push_back(0.005f);
			colors.push_back(grassCol * 1.5f);

			AddSplineTube(vertices, indices, pts, radii, colors);
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)));
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateFlower(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		LSystem lsys;
		if (custom_axiom.empty()) {
			int ruleset = seed % 2;
			if (ruleset == 0) {
				lsys.axiom = "F";
				lsys.rules['F'] = "FF-[+F+F]";
			} else {
				lsys.axiom = "F";
				lsys.rules['F'] = "F[+F]F[-F]F";
			}
		} else {
			lsys.axiom = custom_axiom;
			lsys.rules = ParseRules(custom_rules);
		}

		std::string expanded = lsys.expand(iterations);

		std::stack<TurtleState>   stack;
		TurtleState               current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.04f};
		float                     angle = 0.5f;
		float                     step = 0.4f;
		glm::vec3                 stemCol(0.2f, 0.6f, 0.2f);
		glm::vec3                 petalCol(0.9f + dis(gen), 0.2f + dis(gen), 0.4f + dis(gen));
		std::vector<SDFPrimitive> primitives;

		// We still need a way to collect petals/heads which are NOT voxelized
		std::vector<Vertex>       other_vertices;
		std::vector<unsigned int> other_indices;

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				primitives.push_back({SDFPrimitive::Capsule, current.position, nextPos, current.thickness, stemCol});
				current.position = nextPos;
				current.thickness *= 0.85f;
			} else if (c == '+' || c == '-' || c == '&' || c == '^' || c == '\\' || c == '/') {
				if (c == '+')
					current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(1, 0, 0));
				else if (c == '-')
					current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(1, 0, 0));
				else if (c == '&')
					current.orientation = current.orientation * glm::angleAxis(angle, glm::vec3(0, 0, 1));
				else if (c == '^')
					current.orientation = current.orientation * glm::angleAxis(-angle, glm::vec3(0, 0, 1));
				else if (c == '\\')
					current.orientation = current.orientation * glm::angleAxis(angle, glm::vec3(0, 1, 0));
				else if (c == '/')
					current.orientation = current.orientation * glm::angleAxis(-angle, glm::vec3(0, 1, 0));
			} else if (c == '[') {
				stack.push(current);
			} else if (c == ']') {
				// Flower head (flatter)
				AddPuffball(other_vertices, other_indices, current.position, 0.15f, glm::vec3(1.0f, 0.9f, 0.2f), gen, glm::vec3(1.0f, 0.4f, 1.0f));
				for (int i = 0; i < 6; ++i) {
					glm::quat petalOri = current.orientation * glm::angleAxis(i * 1.04f, glm::vec3(0, 1, 0));
					petalOri = petalOri * glm::angleAxis(1.0f, glm::vec3(1, 0, 0));
					AddLeaf(other_vertices, other_indices, current.position, petalOri, 0.3f, petalCol, true);
				}
				current = stack.top();
				stack.pop();
			}
		}

		auto stemData = VoxelizeSurfaceNet(primitives, glm::vec3(1.0f));
		if (!other_vertices.empty()) {
			Mesh otherMesh(other_vertices, other_indices, {});
			otherMesh.diffuseColor = glm::vec3(1.0f);
			stemData->meshes.push_back(otherMesh);
		}

		return std::make_shared<Model>(stemData, true);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateTree(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		LSystem lsys;
		if (custom_axiom.empty()) {
			int ruleset = seed % 2;
			if (ruleset == 0) {
				lsys.axiom = "X";
				lsys.rules['X'] = "F[&+X][&/X][^-X][^\\X]";
				lsys.rules['F'] = "SFF";
			} else {
				lsys.axiom = "X";
				lsys.rules['X'] = "F[+X][-X][&X][^X]";
				lsys.rules['F'] = "FF";
			}
		} else {
			lsys.axiom = custom_axiom;
			lsys.rules = ParseRules(custom_rules);
		}
		std::string expanded = lsys.expand(iterations);

		std::stack<TurtleState>   stack;
		TurtleState               current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.25f};
		float                     angle = 0.5f;
		float                     step = 0.6f;
		glm::vec3                 woodCol(0.35f, 0.25f, 0.15f);
		glm::vec3                 leafCol(0.1f, 0.45f + dis(gen), 0.1f);
		std::vector<SDFPrimitive> primitives;

		for (char c : expanded) {
			if (c == 'F') {
				// Add some jitter to the path
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				nextPos += current.orientation * glm::vec3(dis(gen) * 0.2f, 0, dis(gen) * 0.2f);
				primitives.push_back({SDFPrimitive::Capsule, current.position, nextPos, current.thickness, woodCol});
				current.position = nextPos;
			} else if (c == '+' || c == '-' || c == '&' || c == '^' || c == '\\' || c == '/') {
				if (c == '+')
					current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(1, 0, 0));
				else if (c == '-')
					current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(1, 0, 0));
				else if (c == '&')
					current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(0, 0, 1));
				else if (c == '^')
					current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(0, 0, 1));
				else if (c == '\\')
					current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(0, 1, 0));
				else if (c == '/')
					current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(0, 1, 0));
			} else if (c == '[') {
				stack.push(current);
				// Area conservation: D^2 = sum d_i^2. For 4 branches, each d = D / 2.
				current.thickness *= 0.5f;
			} else if (c == ']') {
				// Leaf cluster
				if (current.thickness < 0.15f) {
					primitives.push_back({SDFPrimitive::Sphere, current.position, current.position, 0.6f, leafCol});
				}
				current = stack.top();
				stack.pop();
			}
		}

		auto data = VoxelizeSurfaceNet(primitives, glm::vec3(1.0f));
		return std::make_shared<Model>(data, true);
	}

	std::shared_ptr<ModelData> ProceduralGenerator::CreateModelDataFromGeometry(
		const std::vector<Vertex>&       vertices_in,
		const std::vector<unsigned int>& indices_in,
		const glm::vec3&                 diffuseColor
	) {
		std::vector<Vertex>       vertices = vertices_in;
		std::vector<unsigned int> indices = indices_in;

		WeldVertices(vertices, indices);

		auto data = std::make_shared<ModelData>();

		Mesh mesh(vertices, indices, {});
		mesh.diffuseColor = diffuseColor;
		data->meshes.push_back(mesh);

		if (!vertices.empty()) {
			glm::vec3 min = vertices[0].Position;
			glm::vec3 max = vertices[0].Position;
			for (const auto& v : vertices) {
				min = glm::min(min, v.Position);
				max = glm::max(max, v.Position);
			}
			data->aabb = AABB(min, max);
		} else {
			data->aabb = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
		}

		data->model_path = "procedural_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

		return data;
	}

} // namespace Boidsish
