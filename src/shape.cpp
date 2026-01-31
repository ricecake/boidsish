#include "shape.h"

#include <cmath>
#include <vector>

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
	unsigned int            Shape::sphere_ebo_ = 0;
	int                     Shape::sphere_vertex_count_ = 0;
	std::shared_ptr<Shader> Shape::shader = nullptr;

	void Shape::GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const {
		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float  theta = lat * glm::pi<float>() / latitude_segments;
				float  phi = lon * 2 * glm::pi<float>() / longitude_segments;
				float  x = radius * sin(theta) * cos(phi);
				float  y = radius * cos(theta);
				float  z = radius * sin(theta) * sin(phi);
				Vertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::vec3(x, y, z);
				v.TexCoords = glm::vec2((float)lon / longitude_segments, (float)lat / latitude_segments);
				vertices.push_back(v);
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
	}

	void Shape::InitSphereMesh() {
		if (sphere_vao_ != 0)
			return; // Already initialized

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		// We use a dummy instance to call the virtual GetGeometry,
		// but since Shape is abstract, we just use the logic directly or
		// we can make a temporary concrete class if we really wanted to.
		// However, Shape's GetGeometry is already the sphere logic.
		// Since InitSphereMesh is static, we can't use 'this'.
		// I'll just keep the logic here but I could have made it a static helper.

		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float  theta = lat * glm::pi<float>() / latitude_segments;
				float  phi = lon * 2 * glm::pi<float>() / longitude_segments;
				float  x = radius * sin(theta) * cos(phi);
				float  y = radius * cos(theta);
				float  z = radius * sin(theta) * sin(phi);
				Vertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::vec3(x, y, z);
				v.TexCoords = glm::vec2((float)lon / longitude_segments, (float)lat / latitude_segments);
				vertices.push_back(v);
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

		glGenVertexArrays(1, &sphere_vao_);
		glBindVertexArray(sphere_vao_);

		glGenBuffers(1, &sphere_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &sphere_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		// EBO is not needed after this, but we're not deleting it.
		// This is a potential resource leak if not managed, but matches original logic.
	}

	void Shape::DestroySphereMesh() {
		if (sphere_vao_ != 0) {
			glDeleteVertexArrays(1, &sphere_vao_);
			glDeleteBuffers(1, &sphere_vbo_);
			if (sphere_ebo_ != 0) {
				glDeleteBuffers(1, &sphere_ebo_);
			}
			sphere_vao_ = 0;
			sphere_vbo_ = 0;
			sphere_ebo_ = 0;
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
		shader->setFloat("objectAlpha", 1.0f); // Default to opaque for this static helper

		glBindVertexArray(sphere_vao_);
		glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Shape::LookAt(const glm::vec3& target, const glm::vec3& up) {
		rotation_ = glm::quat_cast(glm::inverse(glm::lookAt(glm::vec3(x_, y_, z_), target, up)));
	}
} // namespace Boidsish
