#include "terrain.h"

#include "graphics.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

	std::shared_ptr<Shader> Terrain::terrain_shader_ = nullptr;

	Terrain::Terrain(
		const std::vector<unsigned int>& indices,
		const std::vector<glm::vec3>&    vertices,
		const std::vector<glm::vec3>&    normals,
		const PatchProxy&                proxy
	):
		indices_(indices),
		vertices(vertices),
		normals(normals),
		proxy(proxy),
		vao_(0),
		vbo_(0),
		ebo_(0),
		index_count_(indices.size()),
		managed_by_render_manager_(false) {
		// Constructor now only initializes member variables
		// setupMesh() must be called explicitly to upload to GPU (legacy mode)
		// Or use TerrainRenderManager for batched rendering (preferred)
	}

	Terrain::~Terrain() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (vbo_)
			glDeleteBuffers(1, &vbo_);
		if (ebo_)
			glDeleteBuffers(1, &ebo_);
	}

	std::vector<float> Terrain::GetInterleavedVertexData() const {
		std::vector<float> data;
		data.reserve(vertices.size() * 8);
		for (size_t i = 0; i < vertices.size(); ++i) {
			data.push_back(vertices[i].x);
			data.push_back(vertices[i].y);
			data.push_back(vertices[i].z);
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
			// Dummy texture coordinates
			data.push_back(0.0f);
			data.push_back(0.0f);
		}
		return data;
	}

	void Terrain::setupMesh() {
		if (managed_by_render_manager_) {
			// Skip legacy GPU setup when managed by render manager
			return;
		}

		// Generate interleaved vertex data for GPU upload
		vertex_data_ = GetInterleavedVertexData();

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
		if (managed_by_render_manager_) {
			// Rendering handled by TerrainRenderManager
			return;
		}

		terrain_shader_->use();
		terrain_shader_->setMat4("model", GetModelMatrix());

		glBindVertexArray(vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glDrawElements(GL_PATCHES, index_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Terrain::render(Shader& shader, const glm::mat4& model_matrix) const {
		// Terrain is not meant to be cloned, so this is a no-op
	}

	glm::mat4 Terrain::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		return model;
	}

} // namespace Boidsish
