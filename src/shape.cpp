#include "shape.h"

#include <cmath>
#include <vector>

#include "instance_manager.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Initialize static members
	unsigned int            Shape::sphere_vao_ = 0;
	unsigned int            Shape::sphere_vbo_ = 0;
	int                     Shape::sphere_vertex_count_ = 0;
	std::shared_ptr<Shader> Shape::shader = nullptr;

	void Shape::ConfigureInstancing(unsigned int vao) const {
		glBindVertexArray(vao);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(glm::vec4)));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(2 * sizeof(glm::vec4)));
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(3 * sizeof(glm::vec4)));
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(4 * sizeof(glm::vec4)));

		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);
		glVertexAttribDivisor(7, 1);
		glBindVertexArray(0);
	}

	void Shape::UnconfigureInstancing(unsigned int vao) const {
		glBindVertexArray(vao);
		glVertexAttribDivisor(3, 0);
		glVertexAttribDivisor(4, 0);
		glVertexAttribDivisor(5, 0);
		glVertexAttribDivisor(6, 0);
		glVertexAttribDivisor(7, 0);

		glDisableVertexAttribArray(3);
		glDisableVertexAttribArray(4);
		glDisableVertexAttribArray(5);
		glDisableVertexAttribArray(6);
		glDisableVertexAttribArray(7);
		glBindVertexArray(0);
	}

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
				float theta = lat * glm::pi<float>() / latitude_segments;
				float phi = lon * 2 * glm::pi<float>() / longitude_segments;
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

	void Shape::RenderSphere(
		const glm::vec3& position,
		const glm::vec3& color,
		const glm::vec3& scale,
		const glm::quat& rotation
	) {
		if (sphere_vao_ == 0)
			return;

		shader->use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = model * glm::mat4_cast(rotation);
		model = glm::scale(model, scale);
		shader->setMat4("model", model);
		shader->setVec3("objectColor", color.r, color.g, color.b);

		glBindVertexArray(sphere_vao_);
		glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Shape::LookAt(const glm::vec3& target, const glm::vec3& up) {
		rotation_ = glm::quat_cast(glm::inverse(glm::lookAt(glm::vec3(x_, y_, z_), target, up)));
	}
} // namespace Boidsish
