#include "procedural_generator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <numbers>

namespace Boidsish {

	namespace {
		struct LSystem {
			std::string axiom;
			std::map<char, std::string> rules;

			std::string expand(int iterations) {
				std::string current = axiom;
				for (int i = 0; i < iterations; ++i) {
					std::string next;
					for (char c : current) {
						if (rules.count(c)) next += rules.at(c);
						else next += c;
					}
					current = next;
				}
				return current;
			}
		};

		void AddCylinder(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
						 glm::vec3 p1, glm::vec3 p2, float r1, float r2, glm::vec3 color) {
			const int segments = 6;
			unsigned int baseIndex = vertices.size();

			glm::vec3 dir = p2 - p1;
			if (glm::length(dir) < 0.0001f) return;
			glm::vec3 axis = glm::normalize(dir);

			glm::vec3 perp = (std::abs(axis.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
			glm::vec3 xDir = glm::normalize(glm::cross(perp, axis));
			glm::vec3 yDir = glm::cross(axis, xDir);

			for (int i = 0; i < segments; ++i) {
				float angle = (float)i / segments * 2.0f * (float)std::numbers::pi;
				float c = std::cos(angle);
				float s = std::sin(angle);

				glm::vec3 normal = xDir * c + yDir * s;

				Vertex v1, v2;
				v1.Position = p1 + normal * r1;
				v1.Normal = normal;
				v1.Color = color;
				v1.TexCoords = glm::vec2((float)i / segments, 0.0f);

				v2.Position = p2 + normal * r2;
				v2.Normal = normal;
				v2.Color = color;
				v2.TexCoords = glm::vec2((float)i / segments, 1.0f);

				vertices.push_back(v1);
				vertices.push_back(v2);
			}

			for (int i = 0; i < segments; ++i) {
				unsigned int next = (i + 1) % segments;
				indices.push_back(baseIndex + i * 2);
				indices.push_back(baseIndex + next * 2);
				indices.push_back(baseIndex + i * 2 + 1);

				indices.push_back(baseIndex + i * 2 + 1);
				indices.push_back(baseIndex + next * 2);
				indices.push_back(baseIndex + next * 2 + 1);
			}
		}
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateRock(unsigned int seed) {
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		// Icosahedron vertices
		const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
		std::vector<glm::vec3> pts = {
			{-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
			{ 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
			{ t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
		};

		for (auto& p : pts) {
			p = glm::normalize(p);
			// Jitter
			p += glm::vec3(dis(gen), dis(gen), dis(gen));

			Vertex v;
			v.Position = p;
			v.Normal = glm::normalize(p); // Approximate
			v.Color = glm::vec3(0.4f + dis(gen), 0.4f + dis(gen), 0.4f + dis(gen));
			vertices.push_back(v);
		}

		indices = {
			0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
			1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
			3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
			4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
		};

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(0.5f)));
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateGrass(unsigned int seed) {
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> dis(-0.05f, 0.05f);

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		const int num_blades = 3;
		for (int i = 0; i < num_blades; ++i) {
			float angle = (float)i / num_blades * (float)std::numbers::pi;
			float s = std::sin(angle);
			float c = std::cos(angle);

			unsigned int base = vertices.size();

			Vertex v1, v2, v3;
			v1.Position = glm::vec3(-0.5f * c, 0, -0.5f * s);
			v1.Normal = glm::vec3(0, 1, 0);
			v1.Color = glm::vec3(0.1f, 0.6f + dis(gen), 0.1f);
			v1.TexCoords = glm::vec2(0, 0);

			v2.Position = glm::vec3(0.5f * c, 0, 0.5f * s);
			v2.Normal = glm::vec3(0, 1, 0);
			v2.Color = glm::vec3(0.1f, 0.6f + dis(gen), 0.1f);
			v2.TexCoords = glm::vec2(1, 0);

			v3.Position = glm::vec3(dis(gen), 1.0f + dis(gen), dis(gen));
			v3.Normal = glm::vec3(0, 1, 0);
			v3.Color = glm::vec3(0.2f, 0.8f + dis(gen), 0.2f);
			v3.TexCoords = glm::vec2(0.5f, 1.0f);

			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);

			indices.push_back(base);
			indices.push_back(base + 1);
			indices.push_back(base + 2);

			// Double sided
			indices.push_back(base);
			indices.push_back(base + 2);
			indices.push_back(base + 1);
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(0.1f, 0.8f, 0.1f)));
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateFlower(unsigned int seed) {
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		LSystem lsys;
		lsys.axiom = "F";
		lsys.rules['F'] = "F[+F][-F]";
		std::string expanded = lsys.expand(2);

		std::stack<TurtleState> stack;
		TurtleState current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.05f};
		float angle = 0.5f;
		float step = 0.5f;

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				AddCylinder(vertices, indices, current.position, nextPos, current.thickness, current.thickness * 0.8f, glm::vec3(0.1f, 0.6f, 0.1f));
				current.position = nextPos;
				current.thickness *= 0.8f;
			} else if (c == '+') {
				current.orientation = current.orientation * glm::angleAxis(angle, glm::vec3(1, 0, 0));
			} else if (c == '-') {
				current.orientation = current.orientation * glm::angleAxis(-angle, glm::vec3(1, 0, 0));
			} else if (c == '[') {
				stack.push(current);
			} else if (c == ']') {
				// Add a "petal" at the end of a branch
				unsigned int base = vertices.size();
				float petalSize = 0.2f;
				glm::vec3 pColor(1.0f, 0.2f, 0.5f);
				for (int i = 0; i < 5; ++i) {
					float pa = (float)i / 5.0f * 2.0f * (float)std::numbers::pi;
					glm::vec3 petalPos = current.position + current.orientation * (glm::vec3(std::cos(pa), 0.5f, std::sin(pa)) * petalSize);
					Vertex v;
					v.Position = petalPos;
					v.Normal = glm::normalize(petalPos - current.position);
					v.Color = pColor;
					v.TexCoords = glm::vec2(0.5f);
					vertices.push_back(v);
				}
				for (int i = 0; i < 3; ++i) {
					indices.push_back(base);
					indices.push_back(base + i + 1);
					indices.push_back(base + i + 2);
				}

				current = stack.top();
				stack.pop();
			}
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(1.0f, 0.5f, 0.5f)));
	}

	std::shared_ptr<Model> ProceduralGenerator::GenerateTree(unsigned int seed) {
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> dis(-0.1f, 0.1f);

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		LSystem lsys;
		lsys.axiom = "F";
		lsys.rules['F'] = "FF+[+F-F-F]-[-F+F+F]";
		std::string expanded = lsys.expand(2);

		std::stack<TurtleState> stack;
		TurtleState current = {glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0.15f};
		float angle = 0.4f;
		float step = 1.0f;

		for (char c : expanded) {
			if (c == 'F') {
				glm::vec3 nextPos = current.position + current.orientation * glm::vec3(0, step, 0);
				AddCylinder(vertices, indices, current.position, nextPos, current.thickness, current.thickness * 0.85f, glm::vec3(0.4f, 0.25f, 0.1f));
				current.position = nextPos;
				current.thickness *= 0.85f;
			} else if (c == '+') {
				current.orientation = current.orientation * glm::angleAxis(angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '-') {
				current.orientation = current.orientation * glm::angleAxis(-angle + dis(gen), glm::vec3(1, 0, 0));
			} else if (c == '[') {
				stack.push(current);
			} else if (c == ']') {
				// Leaf cluster
				if (current.thickness < 0.1f) {
					unsigned int base = vertices.size();
					glm::vec3 lColor(0.1f, 0.5f + dis(gen), 0.1f);
					for (int i = 0; i < 4; ++i) {
						float la = (float)i / 4.0f * 2.0f * (float)std::numbers::pi;
						Vertex v;
						v.Position = current.position + glm::vec3(std::cos(la), dis(gen), std::sin(la)) * 0.4f;
						v.Normal = glm::vec3(0, 1, 0);
						v.Color = lColor;
						v.TexCoords = glm::vec2(0.5f);
						vertices.push_back(v);
					}
					indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
					indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 3);
				}

				current = stack.top();
				stack.pop();
			}
		}

		return std::make_shared<Model>(CreateModelDataFromGeometry(vertices, indices, glm::vec3(0.4f, 0.2f, 0.1f)));
	}

	std::shared_ptr<ModelData> ProceduralGenerator::CreateModelDataFromGeometry(
		const std::vector<Vertex>& vertices,
		const std::vector<unsigned int>& indices,
		const glm::vec3& diffuseColor
	) {
		auto data = std::make_shared<ModelData>();

		Mesh mesh(vertices, indices, {});
		mesh.diffuseColor = diffuseColor;
		data->meshes.push_back(mesh);

		// Calculate AABB
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
