#include "terrain.h"

#include "graphics.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

	Terrain::Terrain(
		const std::vector<unsigned int>& indices,
		const std::vector<glm::vec3>&    vertices,
		const std::vector<glm::vec3>&    normals,
		const PatchProxy&                proxy,
		std::shared_ptr<Shader>          shader
	):
		shader_(shader),
		indices_(indices),
		vertices(vertices),
		normals(normals),
		proxy(proxy),
		vao_(0),
		vbo_(0),
		ebo_(0),
		index_count_(indices.size()) {}

	Terrain::~Terrain() {
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteBuffers(1, &ebo_);
	}

	void Terrain::setupMesh() {
		// Generate interleaved vertex data for GPU upload
		vertex_data_.reserve(vertices.size() * 8);
		for (size_t i = 0; i < vertices.size(); ++i) {
			vertex_data_.push_back(vertices[i].x);
			vertex_data_.push_back(vertices[i].y);
			vertex_data_.push_back(vertices[i].z);
			vertex_data_.push_back(normals[i].x);
			vertex_data_.push_back(normals[i].y);
			vertex_data_.push_back(normals[i].z);
			// Dummy texture coordinates
			vertex_data_.push_back(0.0f);
			vertex_data_.push_back(0.0f);
		}

		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glGenBuffers(1, &ebo_);

		glBindVertexArray(vao_);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertex_data_.size() * sizeof(float), &vertex_data_[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), &indices_[0], GL_STATIC_DRAW);

		// Position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Texture coordinate attribute
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		// Clear interleaved data after uploading to GPU, but keep vertices and normals for physics
		vertex_data_.clear();
		indices_.clear();
	}

	void Terrain::render() const {
		shader_->use();
		// Set uniforms if necessary, e.g., model matrix
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		shader_->setMat4("model", model);

		glBindVertexArray(vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glDrawElements(GL_PATCHES, index_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

} // namespace Boidsish
