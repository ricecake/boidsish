#include "shape.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	// Initialize static members
	unsigned int            Shape::sphere_vao_ = 0;
	unsigned int            Shape::sphere_vbo_ = 0;
	int                     Shape::sphere_vertex_count_ = 0;
	std::shared_ptr<Shader> Shape::shader = nullptr;

	void Shape::InitSphereMesh() {
		if (sphere_vao_ != 0)
			return; // Already initialized

		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		std::vector<float>        vertices;
		std::vector<unsigned int> indices;

		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float theta = lat * std::numbers::pi / latitude_segments;
				float phi = lon * 2 * std::numbers::pi / longitude_segments;
				float x = radius * sin(theta) * cos(phi);
				float y = radius * cos(theta);
				float z = radius * sin(theta) * sin(phi);
				// Position
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
				// Normal
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
			}
		}

		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				// CCW winding
				indices.push_back(first);
				indices.push_back(first + 1);
				indices.push_back(second);

				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second + 1);
			}
		}
		sphere_vertex_count_ = indices.size();

		GLuint sphere_ebo;
		glGenVertexArrays(1, &sphere_vao_);
		glBindVertexArray(sphere_vao_);

		glGenBuffers(1, &sphere_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &sphere_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
		// EBO is not needed after this, but we're not deleting it.
		// This is a potential resource leak if not managed, but matches original logic.
	}

	void Shape::DestroySphereMesh() {
		if (sphere_vao_ != 0) {
			glDeleteVertexArrays(1, &sphere_vao_);
			glDeleteBuffers(1, &sphere_vbo_);
			// Note: The EBO is not tracked as a member, so it can't be deleted here.
			// This is a leak from the original code that should be addressed later.
			sphere_vao_ = 0;
			sphere_vbo_ = 0;
			sphere_vertex_count_ = 0;
		}
	}

	void Shape::RenderSphere(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& color, float scale) {
		if (sphere_vao_ == 0)
			return;

		shader->use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);
        model *= glm::mat4_cast(rotation);
		model = glm::scale(model, glm::vec3(scale));
		shader->setMat4("model", model);
		shader->setVec3("objectColor", color.r, color.g, color.b);

		glBindVertexArray(sphere_vao_);
		glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
} // namespace Boidsish
