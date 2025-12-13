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

	Graph::Graph(int id, float x, float y, float z): Shape(id, x, y, z, 1.0f, 1.0f, 1.0f, 1.0f, 0) {}

	Graph::~Graph() {
		if (buffers_initialized_) {
			glDeleteVertexArrays(1, &graph_vao_);
			glDeleteBuffers(1, &graph_vbo_);
		}
		if (dot_buffers_initialized_) {
			glDeleteVertexArrays(1, &dots_vao_);
			glDeleteBuffers(1, &dots_vbo_);
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

	void Graph::SetupBuffers() const {
		if (buffers_initialized_ || edges.empty())
			return;

		struct VertexData {
			glm::vec3 pos, normal, color;
		};

		std::vector<VertexData> all_vertices_data;

		std::map<int, std::vector<int>> adj;
		for (const auto& edge : edges) {
			adj[edge.from_vertex_index].push_back(edge.to_vertex_index);
			adj[edge.to_vertex_index].push_back(edge.from_vertex_index);
		}

		for (const auto& edge : edges) {
			if (edge.from_vertex_index >= (int)vertices.size() || edge.to_vertex_index >= (int)vertices.size())
				continue;

			const auto& v1 = vertices[edge.from_vertex_index];
			const auto& v2 = vertices[edge.to_vertex_index];

			Vertex v0 = v1;
			if (adj.count(edge.from_vertex_index)) {
				for (int n_idx : adj[edge.from_vertex_index]) {
					if (n_idx != edge.to_vertex_index) {
						v0 = vertices[n_idx];
						break;
					}
				}
			}
			if (v0.position.x == v1.position.x && v0.position.y == v1.position.y && v0.position.z == v1.position.z)
				v0.position = v1.position - (v2.position - v1.position);

			Vertex v3 = v2;
			if (adj.count(edge.to_vertex_index)) {
				for (int n_idx : adj[edge.to_vertex_index]) {
					if (n_idx != edge.from_vertex_index) {
						v3 = vertices[n_idx];
						break;
					}
				}
			}
			if (v3.position.x == v2.position.x && v3.position.y == v2.position.y && v3.position.z == v2.position.z)
				v3.position = v2.position + (v2.position - v1.position);

			Vector3 p0 = v0.position, p1 = v1.position, p2 = v2.position, p3 = v3.position;

			std::vector<std::vector<VertexData>> rings;
			Vector3                              last_normal;

			{
				Vector3 point1 = CatmullRom(0.0f, p0, p1, p2, p3);
				Vector3 point2 = CatmullRom(1.0f / CURVE_SEGMENTS, p0, p1, p2, p3);
				Vector3 tangent = (point2 - point1).Normalized();
				if (abs(tangent.y) < 0.999)
					last_normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				else
					last_normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
			}

			for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
				std::vector<VertexData> ring;
				float                   t = (float)i / CURVE_SEGMENTS;

				Vector3   point = CatmullRom(t, p0, p1, p2, p3);
				glm::vec3 color = {(1 - t) * v1.r + t * v2.r, (1 - t) * v1.g + t * v2.g, (1 - t) * v1.b + t * v2.b};
				float     r = ((1 - t) * v1.size + t * v2.size) * EDGE_RADIUS_SCALE;

				Vector3 tangent;
				if (i < CURVE_SEGMENTS) {
					tangent = (CatmullRom((float)(i + 1) / CURVE_SEGMENTS, p0, p1, p2, p3) - point).Normalized();
				} else {
					tangent = (point - CatmullRom((float)(i - 1) / CURVE_SEGMENTS, p0, p1, p2, p3)).Normalized();
				}

				Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
				if (normal.MagnitudeSquared() < 1e-6) {
					if (abs(tangent.y) < 0.999)
						normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
					else
						normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				} else {
					normal.Normalize();
				}
				Vector3 bitangent = tangent.Cross(normal).Normalized();
				last_normal = normal;

				for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
					float   angle = 2.0f * std::numbers::pi * j / CYLINDER_SEGMENTS;
					Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
					Vector3 pos = point + cn * r;
					ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
				}
				rings.push_back(ring);
			}

			if (!all_vertices_data.empty() && !rings.empty()) {
				// Create a degenerate triangle to disconnect from the previous edge's strip.
				all_vertices_data.push_back(all_vertices_data.back());
				all_vertices_data.push_back(rings[0][0]);
			}
			for (int i = 0; i < CURVE_SEGMENTS; ++i) {
				// Create a strip for the segment between ring i and ring i+1.
				for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
					all_vertices_data.push_back(rings[i][j]);
					all_vertices_data.push_back(rings[i + 1][j]);
				}
			}
		}

		edge_vertex_count_ = all_vertices_data.size();

		glGenVertexArrays(1, &graph_vao_);
		glBindVertexArray(graph_vao_);
		glGenBuffers(1, &graph_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, graph_vbo_);
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
		buffers_initialized_ = true;
	}

	void Graph::SetupDotBuffers() const {
		if (dot_buffers_initialized_ || vertices.empty())
			return;

		struct DotData {
			glm::vec3 position;
			float     size;
			glm::vec4 color;
		};
		std::vector<DotData> dot_data;
		dot_data.reserve(vertices.size());
		for (const auto& vertex : vertices) {
			dot_data.push_back({
				glm::vec3(
					vertex.position.x + GetX(),
					vertex.position.y + GetY(),
					vertex.position.z + GetZ()
				),
				vertex.size,
				glm::vec4(vertex.r, vertex.g, vertex.b, vertex.a),
			});
		}

		glGenVertexArrays(1, &dots_vao_);
		glBindVertexArray(dots_vao_);

		glBindBuffer(GL_ARRAY_BUFFER, Shape::sphere_vbo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Shape::sphere_ebo_);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

		glGenBuffers(1, &dots_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, dots_vbo_);
		glBufferData(GL_ARRAY_BUFFER, dot_data.size() * sizeof(DotData), dot_data.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(
			2,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(DotData),
			(void*)offsetof(DotData, position)
		);
		glVertexAttribDivisor(2, 1);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(DotData), (void*)offsetof(DotData, size));
		glVertexAttribDivisor(3, 1);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(
			4,
			4,
			GL_FLOAT,
			GL_FALSE,
			sizeof(DotData),
			(void*)offsetof(DotData, color)
		);
		glVertexAttribDivisor(4, 1);

		glBindVertexArray(0);
		dot_buffers_initialized_ = true;
	}

	void Graph::render() const {
		if (!buffers_initialized_) {
			SetupBuffers();
		}

	// Batch render vertices
	shader->use();
	shader->setBool("useInstancedDrawing", true);
	shader->setBool("useVertexColor", true);
	if (!dot_buffers_initialized_) {
		SetupDotBuffers();
	}

	glBindVertexArray(dots_vao_);
	glDrawElementsInstanced(
		GL_TRIANGLES,
		Shape::sphere_vertex_count_,
		GL_UNSIGNED_INT,
		0,
		vertices.size()
	);
	glBindVertexArray(0);
	shader->setBool("useInstancedDrawing", false);


		shader->use();
		shader->setBool("useVertexColor", true);

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		shader->setMat4("model", model);

	if (buffers_initialized_) {
		glBindVertexArray(graph_vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, edge_vertex_count_);
		glBindVertexArray(0);
	}

		shader->setBool("useVertexColor", false);
		model = glm::mat4(1.0f);
		shader->setMat4("model", model);
	}

} // namespace Boidsish
