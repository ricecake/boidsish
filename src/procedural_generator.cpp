#include "procedural_generator.h"

#include <numbers>
#include <random>

#include "ConfigManager.h"
#include "mesh_optimizer_util.h"
#include "spline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	namespace {
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

		void AddPuffball(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			glm::vec3                  center,
			float                      radius,
			glm::vec3                  color,
			std::mt19937&              gen
		) {
			std::uniform_real_distribution<float> dis(-0.1f * radius, 0.1f * radius);
			unsigned int                          base = vertices.size();
			const int                             lat_segments = 8;
			const int                             lon_segments = 8;

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
					v.Position = center + (normal * radius) + jitter;
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
			glm::vec3                  color
		) {
			unsigned int base = vertices.size();

			// More rounded leaf shape (diamond with midpoints)
			std::vector<glm::vec3> pts = {{0, 0, 0}, {0.3f, 0.5f, 0.1f}, {0, 1.0f, 0}, {-0.3f, 0.5f, -0.1f}};

			for (auto& p : pts) {
				p = pos + ori * (p * size);
				Vertex v;
				v.Position = p;
				v.Normal = ori * glm::vec3(0, 0, 1);
				v.Color = color;
				v.TexCoords = glm::vec2(0.5f);
				vertices.push_back(v);
			}

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

		void AddSplineTube(
			std::vector<Vertex>&          vertices,
			std::vector<unsigned int>&    indices,
			const std::vector<glm::vec3>& pts,
			const std::vector<float>&     radii,
			const std::vector<glm::vec3>& colors
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

			auto v_data = Spline::GenerateTube(s_pts, s_ups, s_sizes, s_colors, false);

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
	} // namespace

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

	std::shared_ptr<Model> ProceduralGenerator::GenerateFlower(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		LSystem lsys;
		lsys.axiom = "F";
		// lsys.rules['F'] = "F[+F][-F]";
		lsys.rules['F'] = "FF-[+F+F]";
		lsys.rules['X'] = "F-[[X]+X]+F[+FX]-X";

		std::string expanded = lsys.expand(2);

		std::stack<TurtleState> stack;
		TurtleState             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.04f};
		float                   angle = 0.5f;
		float                   step = 0.4f;

		glm::vec3 stemCol(0.2f, 0.6f, 0.2f);
		glm::vec3 petalCol(0.9f + dis(gen), 0.2f + dis(gen), 0.4f + dis(gen));

		for (char c : expanded) {
			if (c == 'F') {
				std::vector<glm::vec3> pts = {current.position};
				std::vector<float>     radii = {current.thickness};
				std::vector<glm::vec3> colors = {stemCol};

				current.position += current.orientation * glm::vec3(0, step, 0);
				pts.push_back(current.position);
				current.thickness *= 0.85f;
				radii.push_back(current.thickness);
				colors.push_back(stemCol);

				AddSplineTube(vertices, indices, pts, radii, colors);
			} else if (c == '+') {
				current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '-') {
				current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '&') { // Pitch
				current.orientation = current.orientation * glm::angleAxis(angle, glm::vec3(0, 0, 1));
			} else if (c == '^') {
				current.orientation = current.orientation * glm::angleAxis(-angle, glm::vec3(0, 0, 1));
			} else if (c == '\\') { // Roll
				current.orientation = current.orientation * glm::angleAxis(angle, glm::vec3(0, 1, 0));
			} else if (c == '/') {
				current.orientation = current.orientation * glm::angleAxis(-angle, glm::vec3(0, 1, 0));
			} else if (c == '[') {
				stack.push(current);
			} else if (c == ']') {
				// Flower head
				AddPuffball(vertices, indices, current.position, 0.15f, glm::vec3(1.0f, 0.9f, 0.2f), gen);
				for (int i = 0; i < 6; ++i) {
					glm::quat petalOri = current.orientation * glm::angleAxis(i * 1.04f, glm::vec3(0, 1, 0));
					petalOri = petalOri * glm::angleAxis(1.0f, glm::vec3(1, 0, 0));
					AddLeaf(vertices, indices, current.position, petalOri, 0.3f, petalCol);
				}
				current = stack.top();
				stack.pop();
			}
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)), true);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateTree(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		LSystem lsys;
		lsys.axiom = "X";
		lsys.rules['X'] = "F[&+X][&/X][^-X][^\\X]"; // More 3D branching
		lsys.rules['F'] = "SFF";                    // S for scale
		std::string expanded = lsys.expand(3);

		std::stack<TurtleState> stack;
		TurtleState             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.25f};
		float                   angle = 0.5f;
		float                   step = 0.6f;

		glm::vec3 woodCol(0.35f, 0.25f, 0.15f);
		glm::vec3 leafCol(0.1f, 0.45f + dis(gen), 0.1f);

		for (char c : expanded) {
			if (c == 'F') {
				std::vector<glm::vec3> pts = {current.position};
				std::vector<float>     radii = {current.thickness};
				std::vector<glm::vec3> colors = {woodCol};

				// Add some jitter to the path
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				nextPos += current.orientation * glm::vec3(dis(gen) * 0.2f, 0, dis(gen) * 0.2f);

				current.position = nextPos;
				pts.push_back(current.position);
				radii.push_back(current.thickness);
				colors.push_back(woodCol);

				AddSplineTube(vertices, indices, pts, radii, colors);
			} else if (c == '+') {
				current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '-') {
				current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '&') {
				current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(0, 0, 1));
			} else if (c == '^') {
				current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(0, 0, 1));
			} else if (c == '\\') {
				current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(0, 1, 0));
			} else if (c == '/') {
				current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(0, 1, 0));
			} else if (c == '[') {
				stack.push(current);
				// Area conservation: D^2 = sum d_i^2. For 4 branches, each d = D / 2.
				current.thickness *= 0.5f;
			} else if (c == ']') {
				// Leaf cluster
				if (current.thickness < 0.15f) {
					AddPuffball(vertices, indices, current.position, 0.6f, leafCol, gen);
				}
				current = stack.top();
				stack.pop();
			}
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)), true);
	}

	std::shared_ptr<ModelData> ProceduralGenerator::CreateModelDataFromGeometry(
		const std::vector<Vertex>&       vertices_in,
		const std::vector<unsigned int>& indices_in,
		const glm::vec3&                 diffuseColor
	) {
		auto data = std::make_shared<ModelData>();
		data->model_path = "procedural_" + std::to_string(reinterpret_cast<uintptr_t>(data.get()));

		std::vector<Vertex>       vertices = vertices_in;
		std::vector<unsigned int> indices = indices_in;

		auto& config = ConfigManager::GetInstance();
		if (config.GetAppSettingBool("mesh_simplifier_enabled", false)) {
			float error = config.GetAppSettingFloat("mesh_simplifier_error_procedural", 0.05f);
			float ratio = config.GetAppSettingFloat("mesh_simplifier_target_ratio", 0.5f);
			int   flags = config.GetAppSettingInt("mesh_simplifier_aggression_procedural", 40);
			MeshOptimizerUtil::Simplify(vertices, indices, error, ratio, (unsigned int)flags, data->model_path);
		}

		std::vector<unsigned int> shadow_indices;
		if (config.GetAppSettingBool("mesh_optimizer_enabled", true)) {
			MeshOptimizerUtil::Optimize(vertices, indices, data->model_path);

			if (config.GetAppSettingBool("mesh_optimizer_shadow_indices_enabled", true)) {
				MeshOptimizerUtil::GenerateShadowIndices(vertices, indices, shadow_indices);
			}
		}

		Mesh mesh(vertices, indices, {}, shadow_indices);
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

		return data;
	}

} // namespace Boidsish
