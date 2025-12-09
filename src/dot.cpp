#include "dot.h"
#include <vector>
#include <cmath>
#include <numbers>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"

namespace Boidsish {

	GLuint Dot::vao = 0;
	GLuint Dot::vbo = 0;
	GLuint Dot::ebo = 0;
	int Dot::vertex_count = 0;

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length) {
		this->id = id;
		this->x = x;
		this->y = y;
		this->z = z;
		this->size = size;
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
		this->trail_length = trail_length;
	}

	void Dot::render() const {
		render(*shader);
	}

	void Dot::render(Shader& shader) const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(x, y, z));
		model = glm::scale(model, glm::vec3(size * 0.01f));
		shader.setMat4("model", model);
		shader.setVec3("objectColor", r, g, b);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, vertex_count, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Dot::InitSphereMesh() {
		const int latitude_segments = 16;
		const int longitude_segments = 32;
		const float radius = 1.0f;

		std::vector<float> vertices;
		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float theta = lat * std::numbers::pi / latitude_segments;
				float phi = lon * 2 * std::numbers::pi / longitude_segments;
				float x = radius * sin(theta) * cos(phi);
				float y = radius * cos(theta);
				float z = radius * sin(theta) * sin(phi);
				vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
				vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
			}
		}

		std::vector<unsigned int> indices;
		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				indices.push_back(first); indices.push_back(second); indices.push_back(first + 1);
				indices.push_back(second); indices.push_back(second + 1); indices.push_back(first + 1);
			}
		}
		vertex_count = indices.size();

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	void Dot::CleanupSphereMesh() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ebo);
	}

} // namespace Boidsish
