#include "model.h"

#include <algorithm>
#include <iostream>

#include "asset_manager.h"
#include "logger.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	// Helper to validate VAO ID
	static bool IsValidVAO(GLuint vao) {
		return vao != 0 && glIsVertexArray(vao) == GL_TRUE;
	}

	// Mesh implementation
	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;

		setupMesh();
	}

	void Mesh::setupMesh() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);

		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		// Vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		// Vertex Normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
		// Vertex Texture Coords
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

		glBindVertexArray(0);
	}

	void Mesh::render() const {
		// Check if Shape::shader is valid before rendering
		if (!Shape::shader || !Shape::shader->isValid())
			return;

		if (indices.empty())
			return;

		// Ensure VAO is valid - use glIsVertexArray for thorough check
		if (!IsValidVAO(VAO)) {
			logger::ERROR("Mesh::render() - Invalid VAO {} (glIsVertexArray={})", VAO, glIsVertexArray(VAO));
			return;
		}

		// bind appropriate textures
		unsigned int diffuseNr = 1;
		unsigned int specularNr = 1;
		unsigned int normalNr = 1;
		unsigned int heightNr = 1;

		// Ensure the correct shader is bound before setting uniforms
		Shape::shader->use();

		bool hasDiffuse = false;
		for (const auto& t : textures) {
			if (t.type == "texture_diffuse") {
				hasDiffuse = true;
				break;
			}
		}
		Shape::shader->setBool("use_texture", hasDiffuse);

		Shape::shader->setVec3("objectColor", diffuseColor);
		Shape::shader->setFloat("objectAlpha", opacity);

		for (unsigned int i = 0; i < textures.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i); // active proper texture unit before binding
			// retrieve texture number (the N in diffuse_textureN)
			std::string number;
			std::string name = textures[i].type;
			if (name == "texture_diffuse")
				number = std::to_string(diffuseNr++);
			else if (name == "texture_specular")
				number = std::to_string(specularNr++); // transfer unsigned int to string
			else if (name == "texture_normal")
				number = std::to_string(normalNr++); // transfer unsigned int to string
			else if (name == "texture_height")
				number = std::to_string(heightNr++); // transfer unsigned int to string

			// now set the sampler to the correct texture unit
			Shape::shader->setInt((name + number).c_str(), i);
			// and finally bind the texture
			glBindTexture(GL_TEXTURE_2D, textures[i].id);
		}

		// draw mesh
		glBindVertexArray(VAO);

		// Validate EBO is properly bound after VAO binding
		GLint current_ebo = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
		if (current_ebo == 0) {
			logger::ERROR("Mesh::render() - No EBO bound after VAO {} bind! EBO should be {}", VAO, EBO);
			if (EBO != 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			} else {
				glBindVertexArray(0);
				return;
			}
		}

		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		// always good practice to set everything back to defaults once configured.
		glActiveTexture(GL_TEXTURE0);
	}

	void Mesh::render(Shader& shader) const {
		if (indices.empty())
			return;

		// Ensure VAO is valid - use glIsVertexArray for thorough check
		if (!IsValidVAO(VAO)) {
			logger::ERROR("Mesh::render(shader) - Invalid VAO {} (glIsVertexArray={})", VAO, glIsVertexArray(VAO));
			return;
		}

		bool hasDiffuse = false;
		for (const auto& t : textures) {
			if (t.type == "texture_diffuse") {
				hasDiffuse = true;
				break;
			}
		}
		shader.setBool("use_texture", hasDiffuse);

		shader.setVec3("objectColor", diffuseColor);
		shader.setFloat("objectAlpha", opacity);

		// Bind textures using the provided shader
		bindTextures(shader);

		// draw mesh
		glBindVertexArray(VAO);

		// Validate EBO is properly bound after VAO binding
		GLint current_ebo = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
		if (current_ebo == 0) {
			logger::ERROR("Mesh::render(shader) - No EBO bound after VAO {} bind! EBO should be {}", VAO, EBO);
			if (EBO != 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			} else {
				glBindVertexArray(0);
				return;
			}
		}

		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		// always good practice to set everything back to defaults once configured.
		glActiveTexture(GL_TEXTURE0);
	}

	void Mesh::render_instanced(int count, bool doVAO) const {
		if (indices.empty())
			return;

		if (doVAO) {
			// Validate VAO before instanced render
			if (!IsValidVAO(VAO)) {
				logger::ERROR(
					"Mesh::render_instanced - Invalid VAO {} (glIsVertexArray={})",
					VAO,
					glIsVertexArray(VAO)
				);
				return;
			}
			glBindVertexArray(VAO);
		}

		// Validate EBO is properly bound (should be part of VAO state)
		GLint current_ebo = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
		if (current_ebo == 0) {
			logger::ERROR("Mesh::render_instanced - No EBO bound! VAO={}, EBO={}", VAO, EBO);
			// Attempt recovery if EBO is missing but we know what it should be
			if (EBO != 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			} else {
				if (doVAO)
					glBindVertexArray(0);
				return;
			}
		}

		glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0, count);

		if (doVAO) {
			glBindVertexArray(0);
		}
	}

	void Mesh::bindTextures(Shader& shader) const {
		unsigned int diffuseNr = 1;
		unsigned int specularNr = 1;
		unsigned int normalNr = 1;
		unsigned int heightNr = 1;
		for (unsigned int i = 0; i < textures.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			std::string number;
			std::string name = textures[i].type;
			if (name == "texture_diffuse")
				number = std::to_string(diffuseNr++);
			else if (name == "texture_specular")
				number = std::to_string(specularNr++);
			else if (name == "texture_normal")
				number = std::to_string(normalNr++);
			else if (name == "texture_height")
				number = std::to_string(heightNr++);

			shader.setInt((name + number).c_str(), i);
			glBindTexture(GL_TEXTURE_2D, textures[i].id);
		}
	}

	// Model implementation
	Model::Model(const std::string& path, bool no_cull): no_cull_(no_cull) {
		m_data = AssetManager::GetInstance().GetModelData(path);
	}

	void Model::render() const {
		if (!shader) {
			std::cerr << "Model::render - Shader is not set!" << std::endl;
			return;
		}
		shader->use();
		render(*shader, GetModelMatrix());
	}

	void Model::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (!m_data)
			return;

		// Create model matrix from shape properties
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		model *= glm::mat4_cast(base_rotation_);
		model = glm::scale(model, GetScale());

		shader.setMat4("model", model);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());

		// Set PBR material properties
		shader.setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader.setFloat("roughness", GetRoughness());
			shader.setFloat("metallic", GetMetallic());
			shader.setFloat("ao", GetAO());
		}

		if (this->no_cull_) {
			glDisable(GL_CULL_FACE);
		}

		for (unsigned int i = 0; i < m_data->meshes.size(); i++) {
			m_data->meshes[i].render(shader); // Use the passed shader, not Shape::shader
		}

		if (this->no_cull_) {
			glEnable(GL_CULL_FACE);
		}
	}

	void Model::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (!m_data)
			return;

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model_matrix[3]);

		for (const auto& mesh : m_data->meshes) {
			RenderPacket packet;
			packet.vao = mesh.getVAO();
			packet.vbo = mesh.getVBO();
			packet.ebo = mesh.getEBO();
			packet.index_count = static_cast<unsigned int>(mesh.indices.size());
			packet.draw_mode = GL_TRIANGLES;
			packet.index_type = GL_UNSIGNED_INT;
			packet.shader_id = shader ? shader->ID : 0;

			packet.uniforms.model = model_matrix;
			packet.uniforms.color = glm::vec4(
				GetR() * mesh.diffuseColor.r,
				GetG() * mesh.diffuseColor.g,
				GetB() * mesh.diffuseColor.b,
				GetA() * mesh.opacity
			);
			packet.uniforms.use_pbr = UsePBR();
			packet.uniforms.roughness = GetRoughness();
			packet.uniforms.metallic = GetMetallic();
			packet.uniforms.ao = GetAO();
			packet.uniforms.use_texture = !mesh.textures.empty();
			packet.uniforms.is_colossal = IsColossal();

			packet.casts_shadows = CastsShadows();

			for (const auto& tex : mesh.textures) {
				RenderPacket::TextureInfo info;
				info.id = tex.id;
				info.type = tex.type;
				packet.textures.push_back(info);
			}

			RenderLayer layer = (packet.uniforms.color.w < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(0);

			// Calculate depth for sorting
			float normalized_depth = context.CalculateNormalizedDepth(world_pos);
			packet.sort_key = CalculateSortKey(layer, packet.shader_handle, packet.material_handle, normalized_depth);

			out_packets.push_back(packet);
		}
	}

	glm::mat4 Model::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		model *= glm::mat4_cast(base_rotation_);
		model = glm::scale(model, GetScale());
		return model;
	}

	bool Model::Intersects(const Ray& ray, float& t) const {
		return GetAABB().Intersects(ray, t);
	}

	AABB Model::GetAABB() const {
		if (!m_data)
			return AABB();
		return m_data->aabb.Transform(GetModelMatrix());
	}

	void Model::GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const {
		if (!m_data)
			return;
		unsigned int vertex_offset = 0;
		for (const auto& mesh : m_data->meshes) {
			for (const auto& vertex : mesh.vertices) {
				vertices.push_back(vertex);
			}
			for (unsigned int index : mesh.indices) {
				indices.push_back(index + vertex_offset);
			}
			vertex_offset += static_cast<unsigned int>(mesh.vertices.size());
		}
	}

	const std::vector<Mesh>& Model::getMeshes() const {
		static const std::vector<Mesh> empty;
		return m_data ? m_data->meshes : empty;
	}

	std::string Model::GetInstanceKey() const {
		return m_data ? "Model:" + m_data->model_path : "Model:Unknown";
	}

	const std::string& Model::GetModelPath() const {
		static const std::string empty;
		return m_data ? m_data->model_path : empty;
	}

	unsigned int Model::TextureFromFile(const char* path, const std::string& directory, bool /* gamma */) {
		return AssetManager::GetInstance().GetTexture(path, directory);
	}

} // namespace Boidsish
