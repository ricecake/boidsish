#include "graph.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "dot.h"
#include "shader.h"
#include "spline.h"
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
	}

	void Graph::SetupBuffers() const {
		if (buffers_initialized_ || edges.empty())
			return;

		std::vector<Vector3>   points;
		std::vector<Vector3>   ups;
		std::vector<float>     sizes;
		std::vector<glm::vec3> colors;

		for (const auto& vertex : vertices) {
			points.push_back(vertex.position);
			ups.push_back(Vector3(0, 1, 0)); // Default up vector
			sizes.push_back(vertex.size);
			colors.push_back(glm::vec3(vertex.r, vertex.g, vertex.b));
		}

		auto all_vertices_data = Spline::GenerateTube(points, ups, sizes, colors, false);
		edge_vertex_count_ = all_vertices_data.size();

		glGenVertexArrays(1, &graph_vao_);
		glBindVertexArray(graph_vao_);
		glGenBuffers(1, &graph_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, graph_vbo_);
		glBufferData(
			GL_ARRAY_BUFFER,
			all_vertices_data.size() * sizeof(Spline::VertexData),
			all_vertices_data.data(),
			GL_STATIC_DRAW
		);

		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, pos)
		);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, normal)
		);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			2,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, color)
		);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		buffers_initialized_ = true;
	}

	void Graph::render() const {
		if (!buffers_initialized_) {
			SetupBuffers();
		}

		for (const auto& vertex : vertices) {
			Dot(0,
			    vertex.position.x + GetX(),
			    vertex.position.y + GetY(),
			    vertex.position.z + GetZ(),
			    vertex.size,
			    vertex.r,
			    vertex.g,
			    vertex.b,
			    vertex.a,
			    0)
				.render();
		}

		shader->use();
		shader->setInt("useVertexColor", 1);

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		shader->setMat4("model", model);

		glBindVertexArray(graph_vao_);
		glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
		glBindVertexArray(0);

		shader->setInt("useVertexColor", 0);
		model = glm::mat4(1.0f);
		shader->setMat4("model", model);
	}

} // namespace Boidsish
