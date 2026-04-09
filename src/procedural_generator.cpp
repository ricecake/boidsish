#include "procedural_generator.h"

#include <numbers>
#include <random>

#include "ConfigManager.h"
#include "graphics.h"
#include "mesh_optimizer_util.h"
#include "procedural_mesher.h"
#include "procedural_optimizer.h"
#include "spline.h"
#include "terrain_deformation_manager.h"
#include "terrain_deformations.h"
#include "terrain_generator_interface.h"
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

		struct SpringNode {
			int       id;
			int       parentId;
			glm::vec3 pos;
			glm::vec3 lastPos;
			glm::vec3 velocity = {0, 0, 0};
			float     radius = 0.1f;
			float     maxLength = 1.0f;
			float     currentLength = 0.0f;
			bool      isSplit = false;
			int       generation = 0;
			float     flexibility = 1.0f;
			bool      isEnd = true;
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

			auto v_data =
				Spline::GenerateTube(s_pts, s_ups, s_sizes, s_colors, false, curve_segments, cylinder_segments);

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
		ProceduralIR ir;

		switch (type) {
		case ProceduralType::Rock:
			return GenerateRock(seed);
		case ProceduralType::Grass:
			ir = GenerateGrassIR(seed);
			break;
		case ProceduralType::Flower:
			ir = GenerateFlowerIR(seed);
			break;
		case ProceduralType::Tree:
			ir = GenerateTreeIR(seed);
			break;
		case ProceduralType::TreeSpaceColonization:
			ir = GenerateSpaceColonizationTreeIR(seed);
			break;
		case ProceduralType::TreeSpring:
			ir = GenerateSpringPlantIR(seed);
			break;
		case ProceduralType::Critter:
			ir = GenerateCritterIR(seed);
			break;
		case ProceduralType::Structure:
			ir = GenerateStructureIR(seed);
			break;
		default:
			return nullptr;
		}

		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateSpringPlant(unsigned int seed, const SpringPlantConfig& config) {
		auto ir = GenerateSpringPlantIR(seed, config);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
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

	ProceduralIR ProceduralGenerator::GenerateGrassIR(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		ProceduralIR ir;
		ir.name = "grass";

		const int num_blades = 5;
		for (int i = 0; i < num_blades; ++i) {
			float angle = (float)i / num_blades * 2.0f * (float)std::numbers::pi;
			float s = std::sin(angle);
			float c = std::cos(angle);

			glm::vec3 basePos(c * 0.1f, 0, s * 0.1f);
			glm::vec3 grassCol(0.1f, 0.5f + dis(gen), 0.1f);

			glm::vec3 p1 = basePos;
			float     r1 = 0.05f;
			glm::vec3 c1 = grassCol;

			glm::vec3 p2 = basePos + glm::vec3(dis(gen), 0.5f, dis(gen));
			float     r2 = 0.03f;
			glm::vec3 c2 = grassCol * 1.2f;

			glm::vec3 p3 = basePos + glm::vec3(dis(gen) * 2.0f, 1.0f, dis(gen) * 2.0f);
			float     r3 = 0.005f;
			glm::vec3 c3 = grassCol * 1.5f;

			int id1 = ir.AddTube(p1, p2, r1, r2, c1);
			ir.AddTube(p2, p3, r2, r3, c2, id1);
		}

		return ir;
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateGrass(unsigned int seed) {
		auto ir = GenerateGrassIR(seed);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	ProceduralIR ProceduralGenerator::GenerateFlowerIR(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		ProceduralIR ir;
		ir.name = "flower";

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

		struct TurtleStateIR {
			glm::vec3 position;
			glm::quat orientation;
			float     thickness;
			int       last_node_idx;
			glm::vec3 color;
			int       color_idx;
			int       variant;
		};

		std::vector<glm::vec3> palette = {
			{0.2f, 0.6f, 0.2f}, // Green (stem)
			{0.9f, 0.2f, 0.4f}, // Red/Pink
			{0.9f, 0.9f, 0.2f}, // Yellow
			{0.2f, 0.4f, 0.9f}, // Blue
			{0.6f, 0.2f, 0.8f}, // Purple
			{0.9f, 0.5f, 0.1f}  // Orange
		};

		std::stack<TurtleStateIR> stack;
		TurtleStateIR             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.04f, -1, palette[0], 0, 0};
		float                     angle = 0.5f;
		float                     step = 0.4f;

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 next_pos = current.position + current.orientation * glm::vec3(0, step, 0);
				float     next_thickness = current.thickness * 0.85f;
				int       id = ir.AddTube(
					current.position,
					next_pos,
					current.thickness,
					next_thickness,
					current.color,
					current.last_node_idx
				);
				current.position = next_pos;
				current.thickness = next_thickness;
				current.last_node_idx = id;
			} else if (c == 'L') {
				ir.AddLeaf(
					current.position,
					current.orientation,
					0.3f,
					current.color,
					current.variant,
					current.last_node_idx
				);
			} else if (c == 'P') {
				ir.AddPuffball(current.position, 0.15f, current.color, 0, current.last_node_idx);
			} else if (c == 'B') {
				ir.AddPuffball(current.position, 0.15f, current.color, 1, current.last_node_idx);
			} else if (c == '\'') {
				current.color_idx = (current.color_idx + 1) % palette.size();
				current.color = palette[current.color_idx];
			} else if (c == '!') {
				current.thickness *= 0.8f;
			} else if (isdigit(c)) {
				current.variant = c - '0';
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
				// Legacy flower head if it was just an empty branch ending
				if (custom_axiom.empty()) {
					ir.AddPuffball(current.position, 0.15f, glm::vec3(1.0f, 0.9f, 0.2f), 1, current.last_node_idx);
					for (int i = 0; i < 6; ++i) {
						glm::quat petalOri = current.orientation * glm::angleAxis(i * 1.04f, glm::vec3(0, 1, 0));
						petalOri = petalOri * glm::angleAxis(1.0f, glm::vec3(1, 0, 0));
						ir.AddLeaf(current.position, petalOri, 0.3f, palette[1], 0, current.last_node_idx);
					}
				}
				current = stack.top();
				stack.pop();
			}
		}

		return ir;
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateFlower(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		auto ir = GenerateFlowerIR(seed, custom_axiom, custom_rules, iterations);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateSpaceColonizationTree(unsigned int seed) {
		auto ir = GenerateSpaceColonizationTreeIR(seed);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	ProceduralIR ProceduralGenerator::GenerateSpaceColonizationTreeIR(unsigned int seed) {
		std::mt19937 gen(seed);

		// Parameters
		const float killDistance = 0.5f;
		const float influenceRadius = 1.5f;
		const float growthStep = 0.3f;
		const int   numAttractors = 400;
		const float exponent = 2.0f; // Leonardo's rule

		std::vector<SCAttractor>              attractors;
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
					for (size_t n = 0; n < nodes.size(); ++n) {
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

		ProceduralIR ir;
		ir.name = "sc_tree";
		glm::vec3 woodCol(0.35f, 0.25f, 0.15f);
		glm::vec3 leafCol(0.1f, 0.45f, 0.1f);

		std::vector<int> node_to_ir(nodes.size(), -1);
		for (size_t i = 0; i < nodes.size(); ++i) {
			if (nodes[i].parentId != -1) {
				int parent_ir = node_to_ir[nodes[i].parentId];
				node_to_ir[i] = ir.AddTube(
					nodes[nodes[i].parentId].pos,
					nodes[i].pos,
					nodes[nodes[i].parentId].radius,
					nodes[i].radius,
					woodCol,
					parent_ir
				);
			} else {
				node_to_ir[i] = ir.AddHub(nodes[i].pos, nodes[i].radius, woodCol);
			}
		}

		for (size_t i = 0; i < nodes.size(); ++i) {
			if (nodes[i].children.empty()) {
				ir.AddPuffball(nodes[i].pos, 0.4f, leafCol, node_to_ir[i]);
			}
		}

		return ir;
	}

	ProceduralIR ProceduralGenerator::GenerateSpringPlantIR(unsigned int seed, const SpringPlantConfig& config) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);

		ProceduralIR ir;
		ir.name = "spring_plant";

		std::vector<SpringNode> nodes;
		// Initial root node at origin
		SpringNode root;
		root.id = 0;
		root.parentId = -1;
		root.pos = {0, 0, 0};
		root.lastPos = root.pos;
		root.radius = 0.2f;
		root.generation = 0;
		root.isEnd = false;
		root.isSplit = true;
		nodes.push_back(root);

		// First segment end
		SpringNode firstEnd;
		firstEnd.id = 1;
		firstEnd.parentId = 0;
		firstEnd.pos = {0, 0.1f, 0};
		firstEnd.lastPos = firstEnd.pos;
		firstEnd.radius = 0.15f;
		firstEnd.generation = 1;
		firstEnd.maxLength = config.branch_length_factor;
		firstEnd.currentLength = 0.1f;
		firstEnd.isEnd = true;
		firstEnd.isSplit = false;
		firstEnd.flexibility = 1.0f;
		nodes.push_back(firstEnd);

		int nextId = 2;

		for (int iter = 0; iter < config.iterations; ++iter) {
			// Growth and Simulation Loop
			float dt = 0.02f;
			int   steps = static_cast<int>(config.equilibrium_time / dt);
			if (steps < 1)
				steps = 1;

			for (int step = 0; step < steps; ++step) {
				std::vector<glm::vec3> forces(nodes.size(), glm::vec3(0.0f));

				for (size_t i = 0; i < nodes.size(); ++i) {
					if (nodes[i].parentId == -1)
						continue;

					SpringNode& node = nodes[i];
					SpringNode& parent = nodes[node.parentId];

					// 1. Growth: segments grow over time until they hit maxLength
					if (node.currentLength < node.maxLength) {
						float growth = 0.5f * dt;
						node.currentLength += growth;
						if (node.currentLength > node.maxLength)
							node.currentLength = node.maxLength;

						// Also grow thicker
						node.radius += growth * 0.1f;
					}

					// 2. Spring Force to parent (Distance constraint)
					glm::vec3 toParent = parent.pos - node.pos;
					float     dist = glm::length(toParent);
					if (dist > 0.0001f) {
						float springForce = (dist - node.currentLength) * 50.0f;
						forces[i] += (toParent / dist) * springForce;
					}

					// 3. Upward Pull
					forces[i] += glm::vec3(0, config.up_pull, 0) * node.flexibility;

					// 4. Outward Pull (Away from Y axis)
					glm::vec3 outward = node.pos;
					outward.y = 0;
					if (glm::length(outward) > 0.0001f) {
						forces[i] += glm::normalize(outward) * config.up_pull * 0.5f * node.flexibility;
					}

					// 5. Repulsion from other ends
					if (node.isEnd) {
						for (size_t j = 0; j < nodes.size(); ++j) {
							if (i == j)
								continue;
							if (!nodes[j].isEnd)
								continue;

							glm::vec3 diff = node.pos - nodes[j].pos;
							float     d2 = glm::length2(diff);
							if (d2 < 16.0f && d2 > 0.0001f) {
								forces[i] += (glm::normalize(diff) * config.spring_repulsion) / d2;
							}
						}
					}

					// 6. Curvature and Spiral
					float     angle = node.pos.y * config.spiral + node.generation * 0.5f;
					glm::vec3 spiralForce(std::cos(angle), 0, std::sin(angle));
					forces[i] += spiralForce * config.curvature * node.flexibility;
				}

				// Apply forces
				for (size_t i = 0; i < nodes.size(); ++i) {
					if (nodes[i].parentId == -1)
						continue;
					nodes[i].velocity += forces[i] * dt;
					nodes[i].velocity *= 0.9f; // Damping
					nodes[i].pos += nodes[i].velocity * dt;

					// Ground constraint
					if (nodes[i].pos.y < 0) {
						nodes[i].pos.y = 0;
						nodes[i].velocity.y = 0;
					}
				}
			}

			// Splitting Phase
			std::vector<SpringNode> newNodes;
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (nodes[i].isEnd && !nodes[i].isSplit && nodes[i].currentLength >= nodes[i].maxLength * 0.5f) {
					nodes[i].isSplit = true;
					nodes[i].isEnd = false;
					nodes[i].flexibility = 0.1f; // Reduced flexibility once split

					std::uniform_int_distribution<int> branchDis(config.min_branches, config.max_branches);
					int                                numBranches = branchDis(gen);

					for (int b = 0; b < numBranches; ++b) {
						SpringNode newNode;
						newNode.id = nextId++;
						newNode.parentId = nodes[i].id;
						// Small initial push for branches
						glm::vec3 dir = {0, 1, 0};
						if (nodes[i].parentId != -1) {
							glm::vec3 d = nodes[i].pos - nodes[nodes[i].parentId].pos;
							if (glm::length(d) > 0.001f)
								dir = glm::normalize(d);
						}

						glm::vec3 side = glm::cross(dir, glm::vec3(0, 0, 1));
						if (glm::length(side) < 0.001f)
							side = glm::vec3(1, 0, 0);
						glm::quat q = glm::angleAxis((b - (numBranches - 1) * 0.5f) * 0.5f, side);

						newNode.pos = nodes[i].pos + q * dir * 0.05f;
						newNode.lastPos = newNode.pos;
						newNode.velocity = {0, 0, 0};
						newNode.generation = nodes[i].generation + 1;
						// Max length reduces each generation
						newNode.maxLength = nodes[i].maxLength * config.branch_split_factor;
						newNode.currentLength = 0.05f;
						newNode.isEnd = true;
						newNode.isSplit = false;
						newNode.flexibility = 1.0f;
						newNode.radius = nodes[i].radius * 0.75f;
						newNodes.push_back(newNode);
					}
				}
			}
			if (newNodes.empty() && iter > 0)
				break; // No more growth

			nodes.insert(nodes.end(), newNodes.begin(), newNodes.end());

			// Size limit check
			float maxH = 0;
			for (const auto& n : nodes)
				if (n.pos.y > maxH)
					maxH = n.pos.y;
			if (maxH > config.size_limit)
				break;
		}

		// Thickness adjustment (after growth, make parents thicker)
		for (int i = (int)nodes.size() - 1; i >= 0; --i) {
			if (nodes[i].parentId != -1) {
				SpringNode& parent = nodes[nodes[i].parentId];
				parent.radius = std::max(parent.radius, nodes[i].radius * 1.1f);
			}
		}

		// Convert to IR
		glm::vec3 woodCol(0.35f, 0.25f, 0.15f);
		std::vector<int> nodeToIr(nodes.size(), -1);
		for (size_t i = 0; i < nodes.size(); ++i) {
			if (nodes[i].parentId != -1) {
				nodeToIr[i] = ir.AddTube(
					nodes[nodes[i].parentId].pos,
					nodes[i].pos,
					nodes[nodes[i].parentId].radius,
					nodes[i].radius,
					woodCol,
					nodeToIr[nodes[i].parentId]
				);
			} else {
				nodeToIr[i] = ir.AddHub(nodes[i].pos, nodes[i].radius, woodCol);
			}
		}

		// Leaves and Flowers (fashion that avoids overlap)
		std::vector<glm::vec3> populatedPoints;
		float                  minDist = 0.4f;

		glm::vec3 leafCol(0.1f, 0.45f, 0.1f);
		glm::vec3 flowerCol(0.95f, 0.8f, 0.2f);

		for (int i = (int)nodes.size() - 1; i >= 0; i--) {
			if (nodes[i].generation > 2 || nodes[i].isEnd) {
				bool tooClose = false;
				for (const auto& p : populatedPoints) {
					if (glm::distance(nodes[i].pos, p) < minDist) {
						tooClose = true;
						break;
					}
				}

				if (!tooClose) {
					populatedPoints.push_back(nodes[i].pos);
					if (dis(gen) > 0.8f) {
						ir.AddPuffball(nodes[i].pos, 0.25f, flowerCol, 1, nodeToIr[i]);
					} else {
						glm::quat leafOri = glm::angleAxis(dis(gen) * 6.28f, glm::vec3(0, 1, 0)) *
						                    glm::angleAxis(dis(gen) * 1.57f, glm::vec3(1, 0, 0));
						ir.AddLeaf(nodes[i].pos, leafOri, 0.4f, leafCol, 0, nodeToIr[i]);
					}
				}
			}
		}

		return ir;
	}

	ProceduralIR ProceduralGenerator::GenerateTreeIR(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		ProceduralIR ir;
		ir.name = "tree";

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

		struct TurtleStateIR {
			glm::vec3 position;
			glm::quat orientation;
			float     thickness;
			int       last_node_idx;
		};

		std::stack<TurtleStateIR> stack;
		TurtleStateIR             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.25f, -1};
		float                     angle = 0.5f;
		float                     step = 0.6f;

		glm::vec3 woodCol(0.35f, 0.25f, 0.15f);
		glm::vec3 leafCol(0.1f, 0.45f + dis(gen), 0.1f);

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				nextPos += current.orientation * glm::vec3(dis(gen) * 0.2f, 0, dis(gen) * 0.2f);

				int id = ir.AddTube(
					current.position,
					nextPos,
					current.thickness,
					current.thickness,
					woodCol,
					current.last_node_idx
				);
				current.position = nextPos;
				current.last_node_idx = id;
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
				current.thickness *= 0.707f; // Slightly more conservative area conservation
			} else if (c == ']') {
				if (current.thickness < 0.15f) {
					ir.AddPuffball(current.position, 0.6f, leafCol, current.last_node_idx);
				}
				current = stack.top();
				stack.pop();
			}
		}

		return ir;
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateTree(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		auto ir = GenerateTreeIR(seed, custom_axiom, custom_rules, iterations);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateCritter(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		auto ir = GenerateCritterIR(seed, custom_axiom, custom_rules, iterations);
		ProceduralOptimizer::Optimize(ir);
		return ProceduralMesher::GenerateModel(ir);
	}

	ProceduralIR ProceduralGenerator::GenerateStructureIR(unsigned int seed) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);
		std::uniform_real_distribution<float> sizeDis(1.0f, 3.0f);
		std::uniform_int_distribution<int>    floorDis(1, 5);
		std::uniform_real_distribution<float> colorDis(0.2f, 0.8f);

		ProceduralIR ir;
		ir.name = "structure";

		int   floors = floorDis(gen);
		float width = sizeDis(gen);
		float depth = sizeDis(gen);
		float floorHeight = 1.5f;

		glm::vec3 wallColor(colorDis(gen), colorDis(gen), colorDis(gen));
		glm::vec3 roofColor(colorDis(gen), colorDis(gen), colorDis(gen));
		glm::vec3 windowColor(0.8f, 0.9f, 1.0f);
		glm::vec3 windowEmissive(1.5f, 1.2f, 0.5f);

		// Foundation
		ProceduralElement foundation;
		foundation.type = ProceduralElementType::Box;
		foundation.position = glm::vec3(0, -0.4f, 0);
		foundation.dimensions = glm::vec3(width + 0.2f, 0.4f, depth + 0.2f);
		foundation.color = glm::vec3(0.4f, 0.42f, 0.45f);
		foundation.roughness = 0.9f;
		foundation.metallic = 0.1f;
		ir.AddElement(foundation);

		// Core structure
		for (int i = 0; i < floors; ++i) {
			float     h = floorHeight;
			glm::vec3 pos(0, (i + 0.5f) * h, 0);
			glm::vec3 dims(width, h * 0.5f, depth);

			ProceduralElement e;
			e.type = ProceduralElementType::Box;
			e.position = pos;
			e.dimensions = dims;
			e.color = wallColor;
			e.roughness = 0.8f;
			ir.AddElement(e);

			// Windows
			int numWindowsW = static_cast<int>(width * 2);
			int numWindowsD = static_cast<int>(depth * 2);

			auto addWindow = [&](glm::vec3 wPos, glm::quat wOri) {
				ProceduralElement w;
				w.type = ProceduralElementType::Box;
				w.position = wPos;
				w.orientation = wOri;
				w.dimensions = glm::vec3(0.3f, 0.4f, 0.05f);
				w.color = windowColor;
				w.emissiveColor = windowEmissive;
				w.metallic = 0.9f;
				w.roughness = 0.1f;
				w.ao = 1.0f;
				ir.AddElement(w);
			};

			for (int j = 0; j < numWindowsW; ++j) {
				float x = -width + (j + 0.5f) * (2.0f * width / numWindowsW);
				addWindow(glm::vec3(x, pos.y, depth + 0.01f), glm::quat(1, 0, 0, 0));
				addWindow(glm::vec3(x, pos.y, -depth - 0.01f), glm::quat(1, 0, 0, 0));
			}
			for (int j = 0; j < numWindowsD; ++j) {
				float z = -depth + (j + 0.5f) * (2.0f * depth / numWindowsD);
				addWindow(glm::vec3(width + 0.01f, pos.y, z), glm::angleAxis(1.57f, glm::vec3(0, 1, 0)));
				addWindow(glm::vec3(-width - 0.01f, pos.y, z), glm::angleAxis(1.57f, glm::vec3(0, 1, 0)));
			}
		}

		// Door on first floor
		ProceduralElement door;
		door.type = ProceduralElementType::Box;
		door.position = glm::vec3(0, 0.6f, depth + 0.02f);
		door.dimensions = glm::vec3(0.4f, 0.6f, 0.05f);
		door.color = glm::vec3(0.3f, 0.2f, 0.1f);
		door.roughness = 0.7f;
		ir.AddElement(door);

		// Roof
		int roofStyle = seed % 3;
		if (roofStyle == 0) {
			// Gabled
			ProceduralElement roof;
			roof.type = ProceduralElementType::Wedge;
			roof.position = glm::vec3(0, floors * floorHeight + 0.5f, 0);
			roof.dimensions = glm::vec3(width + 0.2f, 0.5f, depth + 0.1f);
			roof.color = roofColor;
			ir.AddElement(roof);
		} else if (roofStyle == 1) {
			// Pyramidal
			ProceduralElement roof;
			roof.type = ProceduralElementType::Pyramid;
			roof.position = glm::vec3(0, floors * floorHeight + 0.5f, 0);
			roof.dimensions = glm::vec3(width + 0.2f, 0.5f, depth + 0.2f);
			roof.color = roofColor;
			ir.AddElement(roof);
		} else {
			// Flat with border
			ProceduralElement roof;
			roof.type = ProceduralElementType::Box;
			roof.position = glm::vec3(0, floors * floorHeight + 0.05f, 0);
			roof.dimensions = glm::vec3(width + 0.1f, 0.05f, depth + 0.1f);
			roof.color = roofColor;
			ir.AddElement(roof);
		}

		return ir;
	}

	ProceduralIR ProceduralGenerator::GenerateCritterIR(
		unsigned int                    seed,
		const std::string&              custom_axiom,
		const std::vector<std::string>& custom_rules,
		int                             iterations
	) {
		std::mt19937                          gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		ProceduralIR ir;
		ir.name = "critter";

		LSystem lsys;
		if (custom_axiom.empty()) {
			lsys.axiom = "F";
			lsys.rules['F'] = "F[+F]F[-F]F";
		} else {
			lsys.axiom = custom_axiom;
			lsys.rules = ParseRules(custom_rules);
		}

		std::string expanded = lsys.expand(iterations);

		struct TurtleStateIR {
			glm::vec3 position;
			glm::quat orientation;
			float     thickness;
			int       last_node_idx;
			glm::vec3 color;
			int       color_idx;
			int       variant;
		};

		std::vector<glm::vec3> palette = {
			{0.5f, 0.35f, 0.25f}, // Brown
			{0.8f, 0.4f, 0.1f},   // Orange
			{0.2f, 0.2f, 0.2f},   // Dark Grey
			{0.7f, 0.7f, 0.7f},   // Light Grey
			{0.9f, 0.1f, 0.1f}    // Red
		};

		std::stack<TurtleStateIR> stack;
		TurtleStateIR             current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.1f, -1, palette[0], 0, 0};
		float                     angle = 0.4f;
		float                     step = 0.5f;

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 next_pos = current.position + current.orientation * glm::vec3(0, step, 0);
				float     next_thickness = current.thickness * 0.95f;
				int       id = ir.AddTube(
					current.position,
					next_pos,
					current.thickness,
					next_thickness,
					current.color,
					current.last_node_idx
				);
				current.position = next_pos;
				current.thickness = next_thickness;
				current.last_node_idx = id;
			} else if (c == 'L') {
				ir.AddLeaf(
					current.position,
					current.orientation,
					0.4f,
					current.color,
					current.variant,
					current.last_node_idx
				);
			} else if (c == 'P') {
				ir.AddPuffball(current.position, 0.2f, current.color, 0, current.last_node_idx);
			} else if (c == 'B') {
				ir.AddPuffball(current.position, 0.2f, current.color, 1, current.last_node_idx);
			} else if (c == '\'') {
				current.color_idx = (current.color_idx + 1) % palette.size();
				current.color = palette[current.color_idx];
			} else if (c == '!') {
				current.thickness *= 0.8f;
			} else if (isdigit(c)) {
				current.variant = c - '0';
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
				current = stack.top();
				stack.pop();
			}
		}

		return ir;
	}

	void
	ProceduralGenerator::LevelTerrainForModel(Visualizer& viz, std::shared_ptr<Model> model, float blend_distance) {
		if (!model || !model->GetData())
			return;

		glm::vec3 pos = model->GetPosition();
		glm::quat rot = model->GetRotation();
		AABB      aabb = model->GetData()->aabb;

		// Footprint dimensions (unscaled)
		float half_width = (aabb.max.x - aabb.min.x) * 0.5f * model->GetScale().x;
		float half_depth = (aabb.max.z - aabb.min.z) * 0.5f * model->GetScale().z;

		// Calculate rotation in radians (around Y)
		glm::vec3 forward = rot * glm::vec3(0, 0, 1);
		float     rot_y = std::atan2(forward.x, forward.z);

		// Center of the foundation at terrain level
		glm::vec3 center = pos;

		viz.GetTerrain()->AddFlattenSquare(center, half_width, half_depth, blend_distance, rot_y);
		viz.GetTerrain()->InvalidateDeformedChunks();
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
