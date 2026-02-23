#include "terrain.h"

#include "graphics.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

	std::shared_ptr<Shader> Terrain::terrain_shader_ = nullptr;
	ShaderHandle            Terrain::terrain_shader_handle = ShaderHandle(0);

	Terrain::Terrain(
		const std::vector<unsigned int>& indices,
		const std::vector<glm::vec3>&    vertices,
		const std::vector<glm::vec3>&    normals,
		const std::vector<glm::vec2>&    biomes,
		const PatchProxy&                proxy
	):
		indices_(indices),
		vertices(vertices),
		normals(normals),
		biomes(biomes),
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

		// Ensure VAO and shader are valid
		if (vao_ == 0 || !terrain_shader_ || !terrain_shader_->isValid())
			return;

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

	void Terrain::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (managed_by_render_manager_ || vao_ == 0) {
			return;
		}

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model_matrix[3]);

		RenderPacket packet;
		packet.vao = vao_;
		packet.vbo = vbo_;
		packet.ebo = ebo_;
		packet.index_count = static_cast<unsigned int>(index_count_);
		packet.draw_mode = GL_PATCHES;
		packet.index_type = GL_UNSIGNED_INT;
		packet.shader_id = terrain_shader_ ? terrain_shader_->ID : 0;

		packet.uniforms.model = model_matrix;
		packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
		packet.uniforms.use_pbr = UsePBR();
		packet.uniforms.roughness = GetRoughness();
		packet.uniforms.metallic = GetMetallic();
		packet.uniforms.ao = GetAO();
		packet.uniforms.use_texture = false;
		packet.uniforms.is_colossal = IsColossal();

		packet.casts_shadows = CastsShadows();

		RenderLayer layer = RenderLayer::Opaque;

		packet.shader_handle = terrain_shader_handle;
		packet.material_handle = MaterialHandle(0);

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(
			layer,
			packet.shader_handle,
			packet.vao,
			packet.draw_mode,
			packet.index_count > 0,
			packet.material_handle,
			normalized_depth
		);

		out_packets.push_back(packet);
	}

} // namespace Boidsish
