#include "PearShape.h"

#include <cmath>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	unsigned int PearShape::vao_ = 0;
	unsigned int PearShape::vbo_ = 0;
	unsigned int PearShape::ebo_ = 0;
	int          PearShape::vertex_count_ = 0;

	PearShape::PearShape(int id): Shape(id) {
		SetColor(0.82f, 0.71f, 0.55f); // Tan
		SetUsePBR(true);
		SetRoughness(0.9f); // Matte
		SetMetallic(0.0f);
	}

	void PearShape::render() const {
		render(*shader, GetModelMatrix());
	}

	void PearShape::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (vao_ == 0) {
			setupMesh();
		}

		shader.use();
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());

		shader.setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader.setFloat("roughness", GetRoughness());
			shader.setFloat("metallic", GetMetallic());
			shader.setFloat("ao", GetAO());
		}

		glBindVertexArray(vao_);
		glDrawElements(GL_TRIANGLES, vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glm::mat4 PearShape::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale());
		return model;
	}

	void PearShape::GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const {
		const int latitude_segments = 20;
		const int longitude_segments = 30;

		// 1. Generate the "half pear" surface (curved part)
		// We slice it along the X-Y plane, so Z >= 0
		for (int lat = 0; lat <= latitude_segments; ++lat) {
			float theta = lat * glm::pi<float>() / latitude_segments;
			float sin_theta = sin(theta);
			float cos_theta = cos(theta);

			float radius = sin_theta * (1.0f - 0.4f * cos_theta);
			float y = cos_theta;

			for (int lon = 0; lon <= longitude_segments; ++lon) {
				// Phi goes from 0 to PI to make it half
				float phi = lon * glm::pi<float>() / longitude_segments;
				float x = radius * cos(phi);
				float z = radius * sin(phi); // Z is always >= 0

				Vertex v;
				v.Position = glm::vec3(x, y + 1.0f, z);
				v.Normal = glm::normalize(glm::vec3(x, y * 0.5f, z));
				v.TexCoords = glm::vec2((float)lon / longitude_segments, (float)lat / latitude_segments);
				vertices.push_back(v);
			}
		}

		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				indices.push_back(first);
				indices.push_back(first + 1);
				indices.push_back(second);

				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second + 1);
			}
		}

		// 2. Generate the flat face (Z=0)
		unsigned int base_index = vertices.size();
		for (int lat = 0; lat <= latitude_segments; ++lat) {
			float theta = lat * glm::pi<float>() / latitude_segments;
			float sin_theta = sin(theta);
			float cos_theta = cos(theta);
			float radius = sin_theta * (1.0f - 0.4f * cos_theta);
			float y = cos_theta;

			// Add two vertices for each latitude to form the strip of the flat face
			// Actually a simple fan or strip from the center would work, but let's keep it simple.
			// Center line of the flat face is x=0, z=0? No, it's a slice.
			// At each latitude, we have x from -radius to +radius, z=0.

			Vertex v1, v2;
			v1.Position = glm::vec3(-radius, y + 1.0f, 0.0f);
			v1.Normal = glm::vec3(0.0f, 0.0f, -1.0f);
			v1.TexCoords = glm::vec2(0.0f, (float)lat / latitude_segments);

			v2.Position = glm::vec3(radius, y + 1.0f, 0.0f);
			v2.Normal = glm::vec3(0.0f, 0.0f, -1.0f);
			v2.TexCoords = glm::vec2(1.0f, (float)lat / latitude_segments);

			vertices.push_back(v1);
			vertices.push_back(v2);
		}

		for (int lat = 0; lat < latitude_segments; ++lat) {
			int first = base_index + lat * 2;
			int second = first + 2;

			// CCW winding for Z=-1 normal
			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}

	void PearShape::setupMesh() const {
		if (vao_ != 0)
			return;

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		GetGeometry(vertices, indices);
		vertex_count_ = indices.size();

		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		glGenBuffers(1, &vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

} // namespace Boidsish
