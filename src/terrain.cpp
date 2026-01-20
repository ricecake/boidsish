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
		const PatchProxy&                proxy,
		int                              chunk_size
	):
		indices_(indices),
		vertices(vertices),
		normals(normals),
		proxy(proxy),
		chunk_size_(chunk_size),
		vao_(0),
		vbo_(0),
		ebo_(0),
		index_count_(indices.size()) {
		// Constructor now only initializes member variables
		// setupMesh() must be called explicitly to upload to GPU
	}

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

	float Terrain::GetHeight(float local_x, float local_z) const {
		// Clamp coordinates to be within the chunk bounds
		local_x = std::max(0.0f, std::min(local_x, (float)chunk_size_));
		local_z = std::max(0.0f, std::min(local_z, (float)chunk_size_));

		int x_floor = static_cast<int>(floor(local_x));
		int z_floor = static_cast<int>(floor(local_z));

		// Ensure we don't go out of bounds for the ceiling
		int x_ceil = std::min(x_floor + 1, chunk_size_);
		int z_ceil = std::min(z_floor + 1, chunk_size_);

		float tx = local_x - x_floor;
		float tz = local_z - z_floor;

		int num_vertices_dim = chunk_size_ + 1;

		// Get the heights of the 4 corner vertices
		float h00 = vertices[x_floor * num_vertices_dim + z_floor].y;
		float h10 = vertices[x_ceil * num_vertices_dim + z_floor].y;
		float h01 = vertices[x_floor * num_vertices_dim + z_ceil].y;
		float h11 = vertices[x_ceil * num_vertices_dim + z_ceil].y;

		// Bilinear interpolation
		float h_bot = glm::mix(h00, h10, tx);
		float h_top = glm::mix(h01, h11, tx);
		return glm::mix(h_bot, h_top, tz);
	}

} // namespace Boidsish
