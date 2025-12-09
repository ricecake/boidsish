#include "graph.h"

#include <cmath>

#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "dot.h"
#include "shader.h"

namespace Boidsish {

	Graph::Graph(int id, float x, float y, float z) {
		this->id = id;
		this->x = x;
		this->y = y;
		this->z = z;
		this->r = 1.0f;
		this->g = 1.0f;
		this->b = 1.0f;
		this->a = 1.0f;
		this->trail_length = 0;
	}

	const int   CYLINDER_SEGMENTS = 12;
	const float EDGE_RADIUS_SCALE = 0.005f;
	const int   CURVE_SEGMENTS = 10;

	Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
		return 0.5f *
			((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
		     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
	}

	void Graph::render() const {
		render(*shader);
	}

	void Graph::render(Shader& shader) const {
		for (const auto& vertex : vertices) {
			Dot(0, vertex.position.x + x, vertex.position.y + y, vertex.position.z + z, vertex.size, vertex.r, vertex.g, vertex.b, vertex.a, 0).render(shader);
		}

		std::map<int, std::vector<int>> adj;
		for (const auto& edge : edges) {
			adj[edge.vertex1_idx].push_back(edge.vertex2_idx);
			adj[edge.vertex2_idx].push_back(edge.vertex1_idx);
		}

		std::vector<float> all_vertices;
		std::vector<std::array<float, 4>> all_colors;

		for (const auto& edge : edges) {
			if (edge.vertex1_idx < 0 || edge.vertex1_idx >= (int)vertices.size() || edge.vertex2_idx < 0 ||
			    edge.vertex2_idx >= (int)vertices.size()) {
				continue;
			}

			const auto& v1 = vertices[edge.vertex1_idx];
			const auto& v2 = vertices[edge.vertex2_idx];

			Vertex v0;
			int    v0_idx = -1;
			for (int neighbor_idx : adj[edge.vertex1_idx]) {
				if (neighbor_idx != edge.vertex2_idx) {
					v0_idx = neighbor_idx;
					break;
				}
			}
			if (v0_idx != -1) {
				v0 = vertices[v0_idx];
			} else {
				v0 = v1;
				v0.position = v1.position - (v2.position - v1.position);
			}

			Vertex v3;
			int    v3_idx = -1;
			for (int neighbor_idx : adj[edge.vertex2_idx]) {
				if (neighbor_idx != edge.vertex1_idx) {
					v3_idx = neighbor_idx;
					break;
				}
			}
			if (v3_idx != -1) {
				v3 = vertices[v3_idx];
			} else {
				v3 = v2;
				v3.position = v2.position + (v2.position - v1.position);
			}

			Vector3 p0 = v0.position + Vector3(x,y,z);
			Vector3 p1 = v1.position + Vector3(x,y,z);
			Vector3 p2 = v2.position + Vector3(x,y,z);
			Vector3 p3 = v3.position + Vector3(x,y,z);

			std::vector<Vector3> points;
			std::vector<Vector3> tangents;
			std::vector<std::array<float, 4>> colors;
			std::vector<float> radii;

			for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
				float t = (float)i / CURVE_SEGMENTS;
				points.push_back(CatmullRom(t, p0, p1, p2, p3));

				float u = 1.0f - t;
				colors.push_back({u * v1.r + t * v2.r, u * v1.g + t * v2.g, u * v1.b + t * v2.b, u * v1.a + t * v2.a});
				radii.push_back((u * v1.size + t * v2.size) * EDGE_RADIUS_SCALE);
			}

			if (points.size() < 2) continue;

			for (size_t i = 0; i < points.size(); ++i) {
				if (i == 0)
					tangents.push_back((points[1] - points[0]).Normalized());
				else if (i == points.size() - 1)
					tangents.push_back((points[i] - points[i - 1]).Normalized());
				else
					tangents.push_back((points[i + 1] - points[i - 1]).Normalized());
			}

			Vector3 normal;
			if (std::abs(tangents[0].y) < 0.999f)
				normal = tangents[0].Cross(Vector3(0, 1, 0)).Normalized();
			else
				normal = tangents[0].Cross(Vector3(1, 0, 0)).Normalized();

			for (size_t i = 0; i < points.size(); ++i) {
				if (i > 0) {
					Vector3 t_prev = tangents[i - 1];
					Vector3 t_curr = tangents[i];
					Vector3 axis = t_prev.Cross(t_curr);
					float   angle = acos(std::max(-1.0f, std::min(1.0f, t_prev.Dot(t_curr))));
					if (axis.MagnitudeSquared() > 1e-6) {
						float cos_a = cos(angle);
						float sin_a = sin(angle);
						axis.Normalize();
						normal = normal * cos_a + axis.Cross(normal) * sin_a + axis * axis.Dot(normal) * (1 - cos_a);
					}
				}
				Vector3 bitangent = tangents[i].Cross(normal).Normalized();

				for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
					float   angle = 2.0f * M_PI * (float)j / CYLINDER_SEGMENTS;
					Vector3 circle_normal = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
					Vector3 vertex_pos = points[i] + circle_normal * radii[i];

					all_vertices.push_back(vertex_pos.x);
					all_vertices.push_back(vertex_pos.y);
					all_vertices.push_back(vertex_pos.z);
					all_vertices.push_back(circle_normal.x);
					all_vertices.push_back(circle_normal.y);
					all_vertices.push_back(circle_normal.z);
					all_colors.push_back(colors[i]);
				}
			}
		}

		if (all_vertices.empty()) return;

		GLuint vao, vbo;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, all_vertices.size() * sizeof(float), &all_vertices[0], GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		int vertex_offset = 0;
		for (size_t i = 0; i < edges.size(); ++i) {
			for(int j = 0; j < CURVE_SEGMENTS; ++j) {
				shader.setVec3("objectColor", all_colors[vertex_offset][0], all_colors[vertex_offset][1], all_colors[vertex_offset][2]);
				glDrawArrays(GL_TRIANGLE_STRIP, vertex_offset, 2 * (CYLINDER_SEGMENTS + 1));
				vertex_offset += (CYLINDER_SEGMENTS + 1);
			}
			vertex_offset += (CYLINDER_SEGMENTS + 1);
		}

		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}
} // namespace Boidsish
