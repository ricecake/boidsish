#include "procedural_generator.h"

#include <numbers>
#include <random>

#include "ConfigManager.h"
#include "mesh_optimizer_util.h"
#include "spline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>

namespace Boidsish {

	namespace {
		struct SCNode {
			int              id;
			int              parentId;
			glm::vec3        pos;
			float            radius = 0.1f;
			std::vector<int> children;
			glm::vec3        growthDir = {0, 0, 0};
			int              attractorCount = 0;
		};

		struct SCAttractor {
			glm::vec3 pos;
			bool      active = true;
		};

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
					v.Position = center + (normal * radius * scale);
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
		case ProceduralType::TreeSpaceColonization:
			return GenerateSpaceColonizationTree(seed);
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

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

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

		std::stack<TurtleState> stack;
		TurtleState             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.04f};
		float                   angle = 0.5f;
		float                   step = 0.4f;

		glm::vec3 stemCol(0.2f, 0.6f, 0.2f);
		glm::vec3 petalCol(0.9f + dis(gen), 0.2f + dis(gen), 0.4f + dis(gen));

		std::vector<glm::vec3> branch_pts;
		std::vector<float>     branch_radii;
		std::vector<glm::vec3> branch_cols;

		auto flushBranch = [&]() {
			if (branch_pts.size() >= 2) {
				AddSplineTube(vertices, indices, branch_pts, branch_radii, branch_cols, 4, 6);
			}
			branch_pts.clear();
			branch_radii.clear();
			branch_cols.clear();
		};

		for (char c : expanded) {
			if (c == 'F') {
				if (branch_pts.empty()) {
					branch_pts.push_back(current.position);
					branch_radii.push_back(current.thickness);
					branch_cols.push_back(stemCol);
				}
				current.position += current.orientation * glm::vec3(0, step, 0);
				current.thickness *= 0.85f;
				branch_pts.push_back(current.position);
				branch_radii.push_back(current.thickness);
				branch_cols.push_back(stemCol);
			} else if (c == '+' || c == '-' || c == '&' || c == '^' || c == '\\' || c == '/') {
				// We keep points in the same spline for smooth curves, unless we want sharp joints.
				// For L-systems, rotation usually happens between segments.
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
				flushBranch();
				stack.push(current);
			} else if (c == ']') {
				flushBranch();
				// Flower head
				AddPuffball(vertices, indices, current.position, 0.15f, glm::vec3(1.0f, 0.9f, 0.2f), gen, glm::vec3(1.0f, 0.4f, 1.0f));
				for (int i = 0; i < 6; ++i) {
					glm::quat petalOri = current.orientation * glm::angleAxis(i * 1.04f, glm::vec3(0, 1, 0));
					petalOri = petalOri * glm::angleAxis(1.0f, glm::vec3(1, 0, 0));
					AddLeaf(vertices, indices, current.position, petalOri, 0.3f, petalCol, true);
				}
				current = stack.top();
				stack.pop();
			}
		}
		flushBranch();

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)), true);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateSpaceColonizationTree(unsigned int seed) {
		std::mt19937 gen(seed);

		// Parameters
		const float killDistance = 0.5f;
		const float influenceRadius = 1.5f;
		const float growthStep = 0.3f;
		const int   numAttractors = 400;
		const float exponent = 2.0f; // Leonardo's rule

		std::vector<SCAttractor> attractors;
		std::uniform_real_distribution<float> crownX(-3.0f, 3.0f);
		std::uniform_real_distribution<float> crownY(3.0f, 10.0f);
		std::uniform_real_distribution<float> crownZ(-3.0f, 3.0f);

		for (int i = 0; i < numAttractors; ++i) {
			attractors.push_back({{crownX(gen), crownY(gen), crownZ(gen)}, true});
		}

		std::vector<SCNode> nodes;
		// Root nodes (trunk) to reach the crown
		nodes.push_back({0, -1, {0, 0, 0}});
		int initialTrunkNodes = 10;
		for (int i = 1; i <= initialTrunkNodes; ++i) {
			nodes.push_back({i, i - 1, {0, (float)i * growthStep, 0}});
			nodes[i - 1].children.push_back(i);
		}

		bool growthOccurred = true;
		int  iterations = 0;
		while (growthOccurred && iterations < 200) {
			growthOccurred = false;
			iterations++;

			// Reset growth
			for (auto& n : nodes) {
				n.growthDir = {0, 0, 0};
				n.attractorCount = 0;
			}

			// Association
			for (size_t i = 0; i < attractors.size(); ++i) {
				if (!attractors[i].active)
					continue;

				int   closestNode = -1;
				float minDistSq = influenceRadius * influenceRadius;

				for (size_t n = 0; n < nodes.size(); ++n) {
					float d2 = glm::distance2(attractors[i].pos, nodes[n].pos);
					if (d2 < minDistSq) {
						minDistSq = d2;
						closestNode = (int)n;
					}
				}

				if (closestNode != -1) {
					nodes[closestNode].growthDir += glm::normalize(attractors[i].pos - nodes[closestNode].pos);
					nodes[closestNode].attractorCount++;
				}
			}

			// Growth
			size_t currentSize = nodes.size();
			for (size_t i = 0; i < currentSize; ++i) {
				if (nodes[i].attractorCount > 0) {
					glm::vec3 nextPos = nodes[i].pos + glm::normalize(nodes[i].growthDir) * growthStep;

					// Avoid duplicates/overlapping nodes too close
					bool tooClose = false;
					for(size_t n=0; n<nodes.size(); ++n) {
						if (glm::distance2(nextPos, nodes[n].pos) < (growthStep * growthStep * 0.25f)) {
							tooClose = true;
							break;
						}
					}

					if (!tooClose) {
						SCNode newNode;
						newNode.id = (int)nodes.size();
						newNode.parentId = (int)i;
						newNode.pos = nextPos;
						nodes[i].children.push_back(newNode.id);
						nodes.push_back(newNode);
						growthOccurred = true;
					}
				}
			}

			// Pruning
			for (size_t i = 0; i < attractors.size(); ++i) {
				if (!attractors[i].active)
					continue;
				for (const auto& n : nodes) {
					if (glm::distance2(attractors[i].pos, n.pos) < (killDistance * killDistance)) {
						attractors[i].active = false;
						break;
					}
				}
			}
		}

		// Thickness calculation (Leonardo's Rule)
		for (int i = (int)nodes.size() - 1; i >= 0; --i) {
			if (nodes[i].children.empty()) {
				nodes[i].radius = 0.05f;
			} else {
				float sumArea = 0.0f;
				for (int childIdx : nodes[i].children) {
					sumArea += std::pow(nodes[childIdx].radius, exponent);
				}
				nodes[i].radius = std::pow(sumArea, 1.0f / exponent);
			}
		}

		// Convert to Spline::Graph format
		std::vector<Spline::GraphNode> s_nodes;
		for (const auto& n : nodes) {
			s_nodes.push_back({Vector3(n.pos), n.radius / 0.005f, {0.35f, 0.25f, 0.15f}});
		}

		std::vector<Spline::GraphEdge> s_edges;
		for (const auto& n : nodes) {
			if (n.parentId != -1) {
				s_edges.push_back({n.parentId, n.id});
			}
		}

		// Generate mesh with low poly settings
		auto v_data = Spline::GenerateGraphTube(s_nodes, s_edges, 2, 4);

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		for (const auto& vd : v_data) {
			Vertex v;
			v.Position = vd.pos;
			v.Normal = vd.normal;
			v.Color = vd.color;
			v.TexCoords = {0.5f, 0.5f};
			vertices.push_back(v);
		}
		for (unsigned int i = 0; i < vertices.size(); ++i) indices.push_back(i);

		auto data = CreateModelDataFromGeometry(vertices, indices, {1,1,1});

		// Add some leaves at terminal nodes
		std::vector<Vertex> leaf_vertices;
		std::vector<unsigned int> leaf_indices;
		glm::vec3 leafCol(0.1f, 0.45f, 0.1f);
		for (const auto& n : nodes) {
			if (nodes[n.id].children.empty()) {
				AddPuffball(leaf_vertices, leaf_indices, n.pos, 0.4f, leafCol, gen);
			}
		}

		if (!leaf_vertices.empty()) {
			auto leaf_data = CreateModelDataFromGeometry(leaf_vertices, leaf_indices, {1,1,1});
			for (const auto& mesh : leaf_data->meshes) {
				data->meshes.push_back(mesh);
			}
		}

		return std::make_shared<Model>(data, true);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateTree(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

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

		std::stack<TurtleState> stack;
		TurtleState             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.25f};
		float                   angle = 0.5f;
		float                   step = 0.6f;

		glm::vec3 woodCol(0.35f, 0.25f, 0.15f);
		glm::vec3 leafCol(0.1f, 0.45f + dis(gen), 0.1f);

		std::vector<glm::vec3> branch_pts;
		std::vector<float>     branch_radii;
		std::vector<glm::vec3> branch_cols;

		auto flushBranch = [&]() {
			if (branch_pts.size() >= 2) {
				AddSplineTube(vertices, indices, branch_pts, branch_radii, branch_cols, 6, 8);
			}
			branch_pts.clear();
			branch_radii.clear();
			branch_cols.clear();
		};

		for (char c : expanded) {
			if (c == 'F') {
				if (branch_pts.empty()) {
					branch_pts.push_back(current.position);
					branch_radii.push_back(current.thickness);
					branch_cols.push_back(woodCol);
				}
				// Add some jitter to the path
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				nextPos += current.orientation * glm::vec3(dis(gen) * 0.2f, 0, dis(gen) * 0.2f);

				current.position = nextPos;
				branch_pts.push_back(current.position);
				branch_radii.push_back(current.thickness);
				branch_cols.push_back(woodCol);
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
				flushBranch();
				stack.push(current);
				// Area conservation: D^2 = sum d_i^2. For 4 branches, each d = D / 2.
				current.thickness *= 0.5f;
			} else if (c == ']') {
				flushBranch();
				// Leaf cluster
				if (current.thickness < 0.15f) {
					AddPuffball(vertices, indices, current.position, 0.6f, leafCol, gen);
				}
				current = stack.top();
				stack.pop();
			}
		}
		flushBranch();

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f)), false);
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
