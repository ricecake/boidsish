#include "dot.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	GLuint Dot::sphere_vao_ = 0;
	GLuint Dot::sphere_vbo_ = 0;
	GLuint Dot::sphere_ebo_ = 0;
	int    Dot::sphere_vertex_count_ = 0;

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length):
		Shape(id, x, y, z, r, g, b, a, trail_length), size_(size) {}

	void Dot::render() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = glm::scale(model, glm::vec3(size_ * 0.01f));
		shader->setMat4("model", model);
		shader->setVec3("objectColor", GetR(), GetG(), GetB());

		glBindVertexArray(sphere_vao_);
		glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Dot::InitSphereMesh() {
		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		std::vector<float> vertices;
		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float theta = lat * std::numbers::pi / latitude_segments;
				float phi = lon * 2 * std::numbers::pi / longitude_segments;
				float x = radius * sin(theta) * cos(phi);
				float y = radius * cos(theta);
				float z = radius * sin(theta) * sin(phi);
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
			}
		}

		std::vector<unsigned int> indices;
		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}
		sphere_vertex_count_ = indices.size();

		glGenVertexArrays(1, &sphere_vao_);
		glBindVertexArray(sphere_vao_);

		glGenBuffers(1, &sphere_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &sphere_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	void Dot::CleanupSphereMesh() {
		glDeleteVertexArrays(1, &sphere_vao_);
		glDeleteBuffers(1, &sphere_vbo_);
		glDeleteBuffers(1, &sphere_ebo_);
	}

} // namespace Boidsish
