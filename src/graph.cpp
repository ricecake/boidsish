#include "graph.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "dot.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

	Graph::~Graph() {
		if (buffers_initialized) {
			glDeleteVertexArrays(1, &vao);
			glDeleteBuffers(1, &vbo);
		}
	}

	const int   CYLINDER_SEGMENTS = 12;
	const float EDGE_RADIUS_SCALE = 0.005f;
	const int   CURVE_SEGMENTS = 10;

	Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
		return 0.5f *
			((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
		     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
	}

	void Graph::setup_buffers() {
		if (buffers_initialized || edges.empty())
			return;

		struct VertexData {
			glm::vec3 pos, normal, color;
		};

		std::vector<VertexData> all_vertices_data;

		std::map<int, std::vector<int>> adj;
		for (const auto& edge : edges) {
			adj[edge.vertex1_idx].push_back(edge.vertex2_idx);
			adj[edge.vertex2_idx].push_back(edge.vertex1_idx);
		}

		for (const auto& edge : edges) {
			if (edge.vertex1_idx >= (int)vertices.size() || edge.vertex2_idx >= (int)vertices.size())
				continue;

			const auto& v1 = vertices[edge.vertex1_idx];
			const auto& v2 = vertices[edge.vertex2_idx];

			Vertex v0 = v1;
			if (adj.count(edge.vertex1_idx)) {
				for (int n_idx : adj[edge.vertex1_idx]) {
					if (n_idx != edge.vertex2_idx) {
						v0 = vertices[n_idx];
						break;
					}
				}
			}
			if (v0.position.x == v1.position.x && v0.position.y == v1.position.y && v0.position.z == v1.position.z)
				v0.position = v1.position - (v2.position - v1.position);

			Vertex v3 = v2;
			if (adj.count(edge.vertex2_idx)) {
				for (int n_idx : adj[edge.vertex2_idx]) {
					if (n_idx != edge.vertex1_idx) {
						v3 = vertices[n_idx];
						break;
					}
				}
			}
			if (v3.position.x == v2.position.x && v3.position.y == v2.position.y && v3.position.z == v2.position.z)
				v3.position = v2.position + (v2.position - v1.position);

			Vector3 p0 = v0.position, p1 = v1.position, p2 = v2.position, p3 = v3.position;

			for (int i = 0; i < CURVE_SEGMENTS; ++i) {
				std::vector<Vector3>   p_loop1, p_loop2;
				std::vector<glm::vec3> c_loop1, c_loop2;
				std::vector<Vector3>   n_loop1, n_loop2;

				float t1 = (float)i / CURVE_SEGMENTS;
				float t2 = (float)(i + 1) / CURVE_SEGMENTS;

				Vector3 point1 = CatmullRom(t1, p0, p1, p2, p3), point2 = CatmullRom(t2, p0, p1, p2, p3);

				glm::vec3 color1 =
					{(1 - t1) * v1.r + t1 * v2.r, (1 - t1) * v1.g + t1 * v2.g, (1 - t1) * v1.b + t1 * v2.b};
				glm::vec3 color2 =
					{(1 - t2) * v1.r + t2 * v2.r, (1 - t2) * v1.g + t2 * v2.g, (1 - t2) * v1.b + t2 * v2.b};

				float r1 = ((1 - t1) * v1.size + t1 * v2.size) * EDGE_RADIUS_SCALE,
					  r2 = ((1 - t2) * v1.size + t2 * v2.size) * EDGE_RADIUS_SCALE;

				Vector3 tangent = (point2 - point1).Normalized();
				Vector3 normal, bitangent;
				if (abs(tangent.y) < 0.999)
					normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				else
					normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				bitangent = tangent.Cross(normal).Normalized();

				for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
					float   angle = 2.0f * std::numbers::pi * j / CYLINDER_SEGMENTS;
					Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
					p_loop1.push_back(point1 + cn * r1);
					c_loop1.push_back(color1);
					n_loop1.push_back(cn);
					p_loop2.push_back(point2 + cn * r2);
					c_loop2.push_back(color2);
					n_loop2.push_back(cn);
				}

				for (int j = 0; j < CYLINDER_SEGMENTS; ++j) {
					all_vertices_data.push_back(
						{glm::vec3(p_loop1[j].x, p_loop1[j].y, p_loop1[j].z),
					     glm::vec3(n_loop1[j].x, n_loop1[j].y, n_loop1[j].z),
					     c_loop1[j]}
					);
					all_vertices_data.push_back(
						{glm::vec3(p_loop2[j].x, p_loop2[j].y, p_loop2[j].z),
					     glm::vec3(n_loop2[j].x, n_loop2[j].y, n_loop2[j].z),
					     c_loop2[j]}
					);
					all_vertices_data.push_back(
						{glm::vec3(p_loop1[j + 1].x, p_loop1[j + 1].y, p_loop1[j + 1].z),
					     glm::vec3(n_loop1[j + 1].x, n_loop1[j + 1].y, n_loop1[j + 1].z),
					     c_loop1[j + 1]}
					);

					all_vertices_data.push_back(
						{glm::vec3(p_loop1[j + 1].x, p_loop1[j + 1].y, p_loop1[j + 1].z),
					     glm::vec3(n_loop1[j + 1].x, n_loop1[j + 1].y, n_loop1[j + 1].z),
					     c_loop1[j + 1]}
					);
					all_vertices_data.push_back(
						{glm::vec3(p_loop2[j].x, p_loop2[j].y, p_loop2[j].z),
					     glm::vec3(n_loop2[j].x, n_loop2[j].y, n_loop2[j].z),
					     c_loop2[j]}
					);
					all_vertices_data.push_back(
						{glm::vec3(p_loop2[j + 1].x, p_loop2[j + 1].y, p_loop2[j + 1].z),
					     glm::vec3(n_loop2[j + 1].x, n_loop2[j + 1].y, n_loop2[j + 1].z),
					     c_loop2[j + 1]}
					);
				}
			}
		}

		edge_vertex_count = all_vertices_data.size();

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(
			GL_ARRAY_BUFFER,
			all_vertices_data.size() * sizeof(VertexData),
			all_vertices_data.data(),
			GL_STATIC_DRAW
		);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, pos));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, color));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		buffers_initialized = true;
	}

	void Graph::render() const {
		render(*shader);
	}

	void Graph::render(Shader& shader) const {
		if (!buffers_initialized)
			return;

		for (const auto& vertex : vertices) {
			Dot(0,
			    vertex.position.x + x,
			    vertex.position.y + y,
			    vertex.position.z + z,
			    vertex.size,
			    vertex.r,
			    vertex.g,
			    vertex.b,
			    vertex.a,
			    0)
				.render(shader);
		}

		shader.use();
		shader.setInt("useVertexColor", 1);

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(x, y, z));
		shader.setMat4("model", model);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count);
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);
		model = glm::mat4(1.0f);
		shader.setMat4("model", model);
	}

} // namespace Boidsish
