#include "model.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

#include "animator.h"
#include "asset_manager.h"
#include "logger.h"
#include "shader.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Helper to validate VAO ID
	static bool IsValidVAO(GLuint vao) {
		return vao != 0 && glIsVertexArray(vao) == GL_TRUE;
	}

	// Mesh implementation
	Mesh::Mesh(
		std::vector<Vertex>       vertices,
		std::vector<unsigned int> indices,
		std::vector<Texture>      textures,
		std::vector<unsigned int> shadow_indices
	) {
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;
		this->shadow_indices = shadow_indices;

		setupMesh(nullptr); // Initial setup (legacy if no megabuffer yet)
	}

	Mesh::Mesh(const Mesh& other) {
		vertices = other.vertices;
		indices = other.indices;
		shadow_indices = other.shadow_indices;
		textures = other.textures;
		diffuseColor = other.diffuseColor;
		opacity = other.opacity;
		has_vertex_colors = other.has_vertex_colors;

		// Do not copy VAO/VBO/EBO handles - setupMesh will create new ones if needed
		VAO = VBO = EBO = shadow_EBO = 0;
		allocation.valid = false;
		shadow_allocation.valid = false;
	}

	Mesh& Mesh::operator=(const Mesh& other) {
		if (this != &other) {
			Cleanup();
			vertices = other.vertices;
			indices = other.indices;
			shadow_indices = other.shadow_indices;
			textures = other.textures;
			diffuseColor = other.diffuseColor;
			opacity = other.opacity;
			has_vertex_colors = other.has_vertex_colors;
		}
		return *this;
	}

	void Mesh::setupMesh(Megabuffer* mb) {
		if (mb) {
			if (allocation.valid)
				return;

			// Allocate space for primary geometry
			allocation = mb->AllocateStatic(
				static_cast<uint32_t>(vertices.size()),
				static_cast<uint32_t>(indices.size())
			);
			if (allocation.valid) {
				mb->Upload(
					allocation,
					vertices.data(),
					static_cast<uint32_t>(vertices.size()),
					indices.data(),
					static_cast<uint32_t>(indices.size())
				);
				VAO = mb->GetVAO();

				// If shadow indices are provided, allocate space for them in the same Megabuffer
				if (!shadow_indices.empty()) {
					// Use 0 for vertex_count as we're reusing the primary vertex buffer allocation
					shadow_allocation = mb->AllocateStatic(0, static_cast<uint32_t>(shadow_indices.size()));
					if (shadow_allocation.valid) {
						// Match base_vertex from primary allocation
						shadow_allocation.base_vertex = allocation.base_vertex;
						mb->Upload(
							shadow_allocation,
							nullptr,
							0,
							shadow_indices.data(),
							static_cast<uint32_t>(shadow_indices.size())
						);
					}
				}
			}
			return;
		}

		if (VAO != 0)
			return;

		// Safety check for OpenGL context - skip setup if no context is available (e.g. in tests)
		if (glfwGetCurrentContext() == nullptr) {
			return;
		}

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);

		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		// Combine primary and shadow indices into a single EBO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		size_t primary_size = indices.size() * sizeof(unsigned int);
		size_t shadow_size = shadow_indices.size() * sizeof(unsigned int);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, primary_size + shadow_size, nullptr, GL_STATIC_DRAW);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, primary_size, indices.data());
		if (!shadow_indices.empty()) {
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, primary_size, shadow_size, shadow_indices.data());
			shadow_EBO = EBO; // Reuse the same EBO but with different offset (handled during draw)
		}

		// Vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		// Vertex Normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
		// Vertex Texture Coords
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		// Vertex Color
		glEnableVertexAttribArray(8);
		glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));

		// Bone IDs
		glEnableVertexAttribArray(9);
		glVertexAttribIPointer(9, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

		// Bone Weights
		glEnableVertexAttribArray(10);
		glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

		glBindVertexArray(0);
	}

	void Mesh::Cleanup() {
		if (VAO != 0) {
			glDeleteVertexArrays(1, &VAO);
			VAO = 0;
		}
		if (VBO != 0) {
			glDeleteBuffers(1, &VBO);
			VBO = 0;
		}
		if (EBO != 0) {
			glDeleteBuffers(1, &EBO);
			EBO = 0;
		}
		shadow_EBO = 0;
		allocation.valid = false;
		shadow_allocation.valid = false;
	}

	void Mesh::UploadToGPU() {
		setupMesh(nullptr);
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

		if (allocation.valid) {
			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				static_cast<unsigned int>(indices.size()),
				GL_UNSIGNED_INT,
				(void*)(uintptr_t)(allocation.first_index * sizeof(unsigned int)),
				allocation.base_vertex
			);
		} else {
			glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		}
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

		if (allocation.valid) {
			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				static_cast<unsigned int>(indices.size()),
				GL_UNSIGNED_INT,
				(void*)(uintptr_t)(allocation.first_index * sizeof(unsigned int)),
				allocation.base_vertex
			);
		} else {
			glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		}
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
		if (m_data) {
			m_animator = std::make_unique<Animator>(m_data);
		}
	}

	Model::Model(std::shared_ptr<ModelData> data, bool no_cull): Shape(), m_data(data), no_cull_(no_cull) {
		if (m_data) {
			m_animator = std::make_unique<Animator>(m_data);
		}
	}

	Model::~Model() = default;

	void Model::PrepareResources(Megabuffer* mb) const {
		if (!m_data || !mb)
			return;
		for (auto& mesh : m_data->meshes) {
			mesh.setupMesh(mb);
		}
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

		if (m_animator && !m_data->bone_info_map.empty()) {
			shader.setBool("use_skinning", true);
			// For non-MDI rendering, we upload bones to a uniform array if the shader supports it,
			// or the caller must have set up the SSBO and bone_matrices_offset.
			// To keep it simple and compatible with existing shaders, we'll use uniforms here if needed.
			const auto& boneMatrices = m_animator->GetFinalBoneMatrices();
			for (size_t i = 0; i < boneMatrices.size() && i < 100; i++) {
				std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
				shader.setMat4(name.c_str(), boneMatrices[i]);
			}
		} else {
			shader.setBool("use_skinning", false);
		}

		if (dissolve_enabled_) {
			shader.setBool("dissolve_enabled", true);
			shader.setVec3("dissolve_plane_normal", dissolve_plane_normal_);
			float dist = dissolve_plane_dist_;
			if (use_dissolve_sweep_) {
				// Calculate dMin, dMax on the fly if using sweep
				float     dMin = std::numeric_limits<float>::max();
				float     dMax = -std::numeric_limits<float>::max();
				glm::vec3 n = glm::normalize(dissolve_plane_normal_);

				AABB worldAABB = m_data->aabb.Transform(model);
				// A simple approximation using AABB corners is much faster than all vertices
				for (int j = 0; j < 8; ++j) {
					float d = glm::dot(n, worldAABB.GetCorner(j));
					dMin = std::min(dMin, d);
					dMax = std::max(dMax, d);
				}
				dist = dMin + dissolve_sweep_ * (dMax - dMin);
			}
			shader.setFloat("dissolve_plane_dist", dist);
		} else {
			shader.setBool("dissolve_enabled", false);
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

		float actual_dissolve_dist = dissolve_plane_dist_;
		if (dissolve_enabled_ && use_dissolve_sweep_) {
			float     dMin = std::numeric_limits<float>::max();
			float     dMax = -std::numeric_limits<float>::max();
			glm::vec3 n = glm::normalize(dissolve_plane_normal_);

			AABB worldAABB = m_data->aabb.Transform(model_matrix);
			for (int j = 0; j < 8; ++j) {
				float d = glm::dot(n, worldAABB.GetCorner(j));
				dMin = std::min(dMin, d);
				dMax = std::max(dMax, d);
			}
			actual_dissolve_dist = dMin + dissolve_sweep_ * (dMax - dMin);
		}

		for (const auto& mesh : m_data->meshes) {
			RenderPacket packet;
			packet.vao = mesh.getVAO();
			packet.vbo = mesh.getVBO();
			packet.ebo = mesh.getEBO();
			packet.index_count = static_cast<unsigned int>(mesh.indices.size());

			if (mesh.allocation.valid) {
				packet.base_vertex = mesh.allocation.base_vertex;
				packet.first_index = mesh.allocation.first_index;
			}

			if (mesh.shadow_allocation.valid) {
				packet.shadow_index_count = mesh.shadow_allocation.index_count;
				packet.shadow_first_index = mesh.shadow_allocation.first_index;
			} else if (!mesh.shadow_indices.empty()) {
				packet.shadow_index_count = static_cast<unsigned int>(mesh.shadow_indices.size());
				packet.shadow_first_index = static_cast<uint32_t>(mesh.indices.size());
			}

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

			bool has_diffuse = false;
			for (const auto& tex : mesh.textures) {
				if (tex.type == "texture_diffuse") {
					has_diffuse = true;
					break;
				}
			}

			packet.uniforms.use_texture = has_diffuse ? 1 : 0;
			packet.uniforms.is_colossal = IsColossal();
			packet.uniforms.use_vertex_color = (mesh.has_vertex_colors && !has_diffuse) ? 1 : 0;

			packet.uniforms.dissolve_enabled = dissolve_enabled_ ? 1 : 0;
			packet.uniforms.dissolve_plane_normal = dissolve_plane_normal_;
			packet.uniforms.dissolve_plane_dist = actual_dissolve_dist;

			if (m_animator && !m_data->bone_info_map.empty()) {
				packet.uniforms.use_skinning = 1;
				packet.bone_matrices = m_animator->GetFinalBoneMatrices();
			}
			// Occlusion culling AABB with velocity expansion
			AABB      meshAABB = m_data->aabb.Transform(model_matrix);
			glm::vec3 velocity = world_pos - GetLastPosition();
			if (glm::dot(velocity, velocity) > 0.001f) {
				meshAABB.min = glm::min(meshAABB.min, meshAABB.min - velocity);
				meshAABB.max = glm::max(meshAABB.max, meshAABB.max + velocity);
			}
			packet.uniforms.aabb_min_x = meshAABB.min.x;
			packet.uniforms.aabb_min_y = meshAABB.min.y;
			packet.uniforms.aabb_min_z = meshAABB.min.z;
			packet.uniforms.aabb_max_x = meshAABB.max.x;
			packet.uniforms.aabb_max_y = meshAABB.max.y;
			packet.uniforms.aabb_max_z = meshAABB.max.z;

			packet.casts_shadows = CastsShadows();

			uint32_t texture_hash = 0;
			for (const auto& tex : mesh.textures) {
				RenderPacket::TextureInfo info;
				info.id = tex.id;
				info.type = tex.type;
				packet.textures.push_back(info);
				texture_hash ^= tex.id + 0x9e3779b9 + (texture_hash << 6) + (texture_hash >> 2);
			}

			RenderLayer layer = (packet.uniforms.color.w < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(texture_hash);

			packet.no_cull = no_cull_ || dissolve_enabled_;

			// Calculate depth for sorting
			float normalized_depth = context.CalculateNormalizedDepth(world_pos);
			packet.sort_key = CalculateSortKey(
				layer,
				packet.shader_handle,
				packet.vao,
				packet.draw_mode,
				packet.index_count > 0,
				packet.material_handle,
				normalized_depth,
				packet.no_cull
			);

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

	void Model::SetDissolveSweep(const glm::vec3& direction, float sweep) {
		dissolve_plane_normal_ = direction;
		dissolve_sweep_ = sweep;
		use_dissolve_sweep_ = true;
		dissolve_enabled_ = true;
		MarkDirty();
	}

	void Model::SetAnimation(int index) {
		if (m_animator) {
			m_animator->PlayAnimation(index);
		}
	}

	void Model::SetAnimation(const std::string& name) {
		if (m_animator) {
			m_animator->PlayAnimation(name);
		}
	}

	void Model::UpdateAnimation(float dt) {
		if (m_animator) {
			m_animator->UpdateAnimation(dt);
			MarkDirty();
		}
	}

	void Model::EnsureUniqueModelData() {
		if (m_data.use_count() > 1) {
			m_data = std::make_shared<ModelData>(*m_data);
			if (m_animator) {
				m_animator->SetModelData(m_data);
			}
		}
	}

	void Model::AddBone(const std::string& name, const std::string& parentName, const glm::mat4& localTransform) {
		EnsureUniqueModelData();
		m_data->AddBone(name, parentName, localTransform);
		MarkDirty();
	}

	std::vector<std::string> Model::GetEffectors() const {
		std::vector<std::string> effectors;
		if (!m_data)
			return effectors;

		for (const auto& [name, info] : m_data->bone_info_map) {
			const NodeData* node = m_data->root_node.FindNode(name);
			if (node && node->children.empty()) {
				effectors.push_back(name);
			}
		}
		return effectors;
	}

	void Model::SetBoneConstraint(const std::string& boneName, const BoneConstraint& constraint) {
		EnsureUniqueModelData();
		auto it = m_data->bone_info_map.find(boneName);
		if (it != m_data->bone_info_map.end()) {
			it->second.constraint = constraint;
		}
	}

	BoneConstraint Model::GetBoneConstraint(const std::string& boneName) const {
		if (m_data) {
			auto it = m_data->bone_info_map.find(boneName);
			if (it != m_data->bone_info_map.end()) {
				return it->second.constraint;
			}
		}
		return BoneConstraint();
	}

	glm::vec3 Model::GetBoneWorldPosition(const std::string& boneName) const {
		if (!m_animator)
			return glm::vec3(0.0f);
		glm::mat4 modelMatrix = GetModelMatrix();
		glm::mat4 modelSpace = m_animator->GetBoneModelSpaceTransform(boneName);
		return glm::vec3(modelMatrix * modelSpace * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	}

	void Model::SetBoneWorldPosition(const std::string& boneName, const glm::vec3& worldPos) {
		if (!m_animator)
			return;
		glm::mat4 modelMatrix = GetModelMatrix();
		glm::mat4 invModel = glm::inverse(modelMatrix);
		glm::vec3 modelPos = glm::vec3(invModel * glm::vec4(worldPos, 1.0f));

		std::string parentName = m_animator->GetBoneParentName(boneName);
		glm::mat4   parentModelSpace = glm::mat4(1.0f);
		if (!parentName.empty()) {
			parentModelSpace = m_animator->GetBoneModelSpaceTransform(parentName);
		}

		glm::mat4 invParentModelSpace = glm::inverse(parentModelSpace);
		glm::vec3 localPos = glm::vec3(invParentModelSpace * glm::vec4(modelPos, 1.0f));

		glm::mat4 localTransform = m_animator->GetBoneLocalTransform(boneName);
		localTransform[3] = glm::vec4(localPos, 1.0f);
		m_animator->SetBoneLocalTransform(boneName, localTransform);
		MarkDirty();
	}

	void Model::SkinToHierarchy() {
		if (!m_data || !m_animator)
			return;

		EnsureUniqueModelData();
		m_animator->UpdateAnimation(0.0f); // Make sure global matrices are current

		struct BoneData {
			std::string name;
			int         id;
			glm::vec3   start;
			glm::vec3   end;
		};

		std::vector<BoneData> bones;

		for (const auto& [name, info] : m_data->bone_info_map) {
			BoneData bd;
			bd.name = name;
			bd.id = info.id;
			bd.start = glm::vec3(m_animator->GetBoneModelSpaceTransform(name)[3]);

			// Try to find end from children
			const NodeData* node = m_data->root_node.FindNode(name);
			if (node && !node->children.empty()) {
				bd.end = glm::vec3(m_animator->GetBoneModelSpaceTransform(node->children[0].name)[3]);
			} else {
				bd.end = bd.start; // Effector or no children
			}
			bones.push_back(bd);
		}

		for (auto& mesh : m_data->meshes) {
			for (auto& vertex : mesh.vertices) {
				// Reset bone data
				for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
					vertex.m_BoneIDs[i] = -1;
					vertex.m_Weights[i] = 0.0f;
				}

				// Simple proximity to bone segment
				struct Candidate {
					int   id;
					float dist;
				};

				std::vector<Candidate> candidates;

				for (const auto& bone : bones) {
					float d = 0;
					if (glm::distance(bone.start, bone.end) < 0.001f) {
						d = glm::distance(vertex.Position, bone.start);
					} else {
						// Point-line segment distance
						glm::vec3 ba = bone.end - bone.start;
						glm::vec3 pa = vertex.Position - bone.start;
						float     h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
						d = glm::length(pa - ba * h);
					}
					candidates.push_back({bone.id, d});
				}

				std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
					return a.dist < b.dist;
				});

				// Assign top weights (simple falloff)
				float totalInvDist = 0;
				int   numWeights = 0;
				for (int i = 0; i < MAX_BONE_INFLUENCE && i < (int)candidates.size(); ++i) {
					if (candidates[i].dist < 5.0f) { // Only weight nearby bones
						float w = 1.0f / (candidates[i].dist + 0.001f);
						totalInvDist += w;
						numWeights++;
					}
				}

				if (numWeights > 0) {
					for (int i = 0; i < numWeights; ++i) {
						vertex.m_BoneIDs[i] = candidates[i].id;
						vertex.m_Weights[i] = (1.0f / (candidates[i].dist + 0.001f)) / totalInvDist;
					}
				} else if (!candidates.empty()) {
					// Fallback to closest bone if none are "nearby"
					vertex.m_BoneIDs[0] = candidates[0].id;
					vertex.m_Weights[0] = 1.0f;
				}
			}
			mesh.Cleanup();
			mesh.UploadToGPU();
		}
	}

	void Model::ResetBones() {
		if (m_animator) {
			m_animator->ResetLocalOverrides();
		}
		MarkDirty();
	}

	void Model::SolveIK(
		const std::string&              effectorName,
		const glm::vec3&                targetWorldPos,
		float                           tolerance,
		int                             maxIterations,
		const std::string&              rootBoneName,
		const std::vector<std::string>& lockedBones
	) {
		SolveIK(
			effectorName,
			targetWorldPos,
			glm::quat(0, 0, 0, 0),
			tolerance,
			maxIterations,
			rootBoneName,
			lockedBones
		);
	}

	void Model::SolveIK(
		const std::string&              effectorName,
		const glm::vec3&                targetWorldPos,
		const glm::quat&                targetWorldRot,
		float                           tolerance,
		int                             maxIterations,
		const std::string&              rootBoneName,
		const std::vector<std::string>& lockedBones
	) {
		if (!m_animator || !m_data)
			return;

		std::vector<std::string> chain;
		std::string              current = effectorName;
		while (!current.empty()) {
			chain.push_back(current);
			if (current == rootBoneName)
				break;
			current = m_animator->GetBoneParentName(current);
		}
		if (chain.size() < 2)
			return;

		std::reverse(chain.begin(), chain.end());

		glm::mat4 invModel = glm::inverse(GetModelMatrix());
		glm::vec3 targetPos = glm::vec3(invModel * glm::vec4(targetWorldPos, 1.0f));

		std::vector<glm::vec3> positions;
		std::vector<float>     lengths;
		for (const auto& bone : chain) {
			glm::mat4 ms = m_animator->GetBoneModelSpaceTransform(bone);
			positions.push_back(glm::vec3(ms[3]));
		}
		for (size_t i = 0; i < positions.size() - 1; ++i) {
			lengths.push_back(std::max(0.0001f, glm::distance(positions[i], positions[i + 1])));
		}

		glm::vec3 rootPos = positions[0];
		float     totalLength = 0;
		for (float l : lengths)
			totalLength += l;

		std::set<std::string> lockedSet(lockedBones.begin(), lockedBones.end());

		if (glm::distance(rootPos, targetPos) > totalLength) {
			// Target out of reach
			for (size_t i = 0; i < positions.size() - 1; ++i) {
				float r = glm::distance(targetPos, positions[i]);
				if (r < 0.001f) {
					positions[i + 1] = positions[i] + glm::vec3(0, 0.001f, 0);
					r = 0.001f;
				}
				float l = lengths[i] / r;
				positions[i + 1] = (1.0f - l) * positions[i] + l * targetPos;
			}
		} else {
			for (int iter = 0; iter < maxIterations; ++iter) {
				if (glm::distance(positions.back(), targetPos) < tolerance)
					break;

				// Backward
				positions.back() = targetPos;
				for (int i = (int)positions.size() - 2; i >= 0; --i) {
					if (lockedSet.count(chain[i]))
						continue;
					float r = glm::distance(positions[i + 1], positions[i]);
					if (r < 0.0001f) {
						positions[i] = positions[i + 1] + glm::vec3(0, 0.0001f, 0);
						r = 0.0001f;
					}
					float l = lengths[i] / r;
					positions[i] = (1.0f - l) * positions[i + 1] + l * positions[i];
				}

				// Forward
				positions[0] = rootPos;
				for (size_t i = 0; i < positions.size() - 1; ++i) {
					if (lockedSet.count(chain[i + 1]))
						continue;
					float r = glm::distance(positions[i + 1], positions[i]);
					if (r < 0.0001f) {
						positions[i + 1] = positions[i] + glm::vec3(0, 0.0001f, 0);
						r = 0.0001f;
					}
					float l = lengths[i] / r;
					positions[i + 1] = (1.0f - l) * positions[i] + l * positions[i + 1];

					// Apply constraints
					const auto& constraint = GetBoneConstraint(chain[i]);
					if (constraint.type != ConstraintType::None) {
						glm::vec3 prevPos = (i == 0) ? (positions[0] + glm::vec3(0, 1, 0)) : positions[i - 1];
						glm::vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
						glm::vec3 parentDir = glm::normalize(positions[i] - prevPos);

						if (constraint.type == ConstraintType::Hinge) {
							glm::vec3 planeNormal = constraint.axis;
							// Project dir onto plane
							glm::vec3 projected = dir - glm::dot(dir, planeNormal) * planeNormal;
							if (glm::length(projected) < 0.001f) {
								projected = glm::cross(planeNormal, glm::vec3(0, 1, 0));
								if (glm::length(projected) < 0.001f)
									projected = glm::cross(planeNormal, glm::vec3(1, 0, 0));
							}
							projected = glm::normalize(projected);

							// Angle limit
							float     dot = glm::clamp(glm::dot(projected, parentDir), -1.0f, 1.0f);
							float     angle = glm::degrees(std::acos(dot));
							glm::vec3 side = glm::cross(parentDir, projected);
							if (glm::dot(side, planeNormal) < 0)
								angle = -angle;

							angle = glm::clamp(angle, constraint.minAngle, constraint.maxAngle);
							glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(angle), planeNormal);
							dir = glm::normalize(glm::vec3(rot * glm::vec4(parentDir, 0.0f)));
						} else if (constraint.type == ConstraintType::Cone) {
							float dot = glm::clamp(glm::dot(dir, parentDir), -1.0f, 1.0f);
							float angle = glm::degrees(std::acos(dot));
							if (angle > constraint.coneAngle) {
								glm::vec3 axis = glm::cross(parentDir, dir);
								if (glm::length(axis) < 0.001f) {
									axis = glm::cross(parentDir, glm::vec3(0, 1, 0));
									if (glm::length(axis) < 0.001f)
										axis = glm::cross(parentDir, glm::vec3(1, 0, 0));
								}
								axis = glm::normalize(axis);
								glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(constraint.coneAngle), axis);
								dir = glm::normalize(glm::vec3(rot * glm::vec4(parentDir, 0.0f)));
							}
						}
						positions[i + 1] = positions[i] + dir * lengths[i];
					}
				}
			}
		}

		// Apply positions back to animator
		// We re-traverse from root to effector to compute consistent local rotations
		glm::mat4   currentParentGlobal = glm::mat4(1.0f);
		std::string parentOfRoot = m_animator->GetBoneParentName(chain[0]);
		if (!parentOfRoot.empty()) {
			currentParentGlobal = m_animator->GetBoneModelSpaceTransform(parentOfRoot);
		}

		for (size_t i = 0; i < positions.size() - 1; ++i) {
			glm::vec3 start = positions[i];
			glm::vec3 end = positions[i + 1];
			glm::vec3 dir = glm::normalize(end - start);

			if (glm::any(glm::isnan(dir)))
				dir = glm::vec3(0, 1, 0);

			// Reference direction in bind space (usually along an axis)
			glm::mat4 bindLocal = m_data->root_node.FindNode(chain[i])->transformation;
			glm::vec3 bindPos = glm::vec3(bindLocal[3]);

			// Find end position in bind space from child
			glm::vec3 bindEndPos = glm::vec3(m_data->root_node.FindNode(chain[i + 1])->transformation[3]);
			glm::vec3 bindDir = glm::normalize(bindEndPos); // Relative to chain[i]

			if (glm::any(glm::isnan(bindDir)))
				bindDir = glm::vec3(0, 1, 0);

			// Calculate new global rotation for this bone
			// It should point from positions[i] to positions[i+1]
			// We use a simple look-at approach or rotation-between-vectors
			glm::quat q = glm::rotation(bindDir, dir);

			if (glm::any(glm::isnan(q)))
				q = glm::quat(1, 0, 0, 0);

			// New global transform for this bone
			glm::mat4 newGlobal = glm::translate(glm::mat4(1.0f), start) * glm::toMat4(q);

			// Convert to local relative to updated parent
			glm::mat4 local = glm::inverse(currentParentGlobal) * newGlobal;

			// Keep original scale
			glm::mat4 oldLocal = m_animator->GetBoneLocalTransform(chain[i]);
			glm::vec3 scale(glm::length(oldLocal[0]), glm::length(oldLocal[1]), glm::length(oldLocal[2]));

			// Reconstruct local with position, rotation and scale
			glm::vec3 localPos = glm::vec3(local[3]);
			glm::quat localRot = glm::quat_cast(local);
			local = glm::translate(glm::mat4(1.0f), localPos) * glm::toMat4(localRot) *
				glm::scale(glm::mat4(1.0f), scale);

			m_animator->SetBoneLocalTransform(chain[i], local);

			// Update parent global for next iteration
			currentParentGlobal = newGlobal;
		}

		// Handle effector orientation if provided
		if (targetWorldRot != glm::quat(0, 0, 0, 0)) {
			SetBoneWorldRotation(effectorName, targetWorldRot);
		}

		UpdateAnimation(0.0f); // Refresh matrices
	}

	glm::quat Model::GetBoneWorldRotation(const std::string& boneName) const {
		if (!m_animator)
			return glm::quat(1, 0, 0, 0);
		glm::mat4 modelMatrix = GetModelMatrix();
		glm::mat4 modelSpace = m_animator->GetBoneModelSpaceTransform(boneName);
		return glm::quat_cast(modelMatrix * modelSpace);
	}

	void Model::SetBoneWorldRotation(const std::string& boneName, const glm::quat& worldRot) {
		if (!m_animator)
			return;
		glm::mat4 modelMatrix = GetModelMatrix();
		glm::mat4 invModel = glm::inverse(modelMatrix);
		glm::quat modelRot = glm::quat_cast(invModel) * worldRot;

		std::string parentName = m_animator->GetBoneParentName(boneName);
		glm::quat   parentModelRot = glm::quat(1, 0, 0, 0);
		if (!parentName.empty()) {
			parentModelRot = glm::quat_cast(m_animator->GetBoneModelSpaceTransform(parentName));
		}

		glm::quat localRot = glm::inverse(parentModelRot) * modelRot;

		glm::mat4 localTransform = m_animator->GetBoneLocalTransform(boneName);
		glm::vec3 scale = glm::vec3(
			glm::length(localTransform[0]),
			glm::length(localTransform[1]),
			glm::length(localTransform[2])
		);
		glm::vec3 pos = glm::vec3(localTransform[3]);

		localTransform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(localRot) *
			glm::scale(glm::mat4(1.0f), scale);
		m_animator->SetBoneLocalTransform(boneName, localTransform);
		MarkDirty();
	}

	unsigned int Model::TextureFromFile(const char* path, const std::string& directory, bool /* gamma */) {
		return AssetManager::GetInstance().GetTexture(path, directory);
	}

	glm::vec3 ModelSlice::GetRandomPoint() const {
		if (triangles.empty())
			return glm::vec3(0.0f);

		// 1. Calculate areas of all triangles
		std::vector<float> areas;
		float              totalArea = 0.0f;
		for (size_t i = 0; i < triangles.size(); i += 3) {
			glm::vec3 a = triangles[i];
			glm::vec3 b = triangles[i + 1];
			glm::vec3 c = triangles[i + 2];
			float     area = 0.5f * glm::length(glm::cross(b - a, c - a));
			areas.push_back(area);
			totalArea += area;
		}

		if (totalArea <= 0.00001f)
			return triangles[0];

		// 2. Pick a random triangle based on area
		float  r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * totalArea;
		size_t pickedTriangle = 0;
		for (size_t i = 0; i < areas.size(); ++i) {
			r -= areas[i];
			if (r <= 0) {
				pickedTriangle = i;
				break;
			}
		}

		// 3. Pick a random point in the picked triangle
		float u = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		float v = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		if (u + v > 1.0f) {
			u = 1.0f - u;
			v = 1.0f - v;
		}

		size_t idx = pickedTriangle * 3;
		return triangles[idx] + u * (triangles[idx + 1] - triangles[idx]) + v * (triangles[idx + 2] - triangles[idx]);
	}

	ModelSlice Model::GetSlice(const glm::vec3& direction, float scale) const {
		ModelSlice slice;
		if (!m_data)
			return slice;

		float     mag = glm::length(direction);
		glm::vec3 n = (mag > 0.0001f) ? glm::normalize(direction) : glm::vec3(0, 1, 0);

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		GetGeometry(vertices, indices);

		if (vertices.empty())
			return slice;

		// Transform vertices to world space
		glm::mat4 modelMatrix = GetModelMatrix();
		for (auto& v : vertices) {
			v.Position = glm::vec3(modelMatrix * glm::vec4(v.Position, 1.0f));
		}

		// Find min/max distance along n
		float dMin = std::numeric_limits<float>::max();
		float dMax = -std::numeric_limits<float>::max();
		for (const auto& v : vertices) {
			float d = glm::dot(n, v.Position);
			dMin = std::min(dMin, d);
			dMax = std::max(dMax, d);
		}

		float dTarget = dMin + scale * (dMax - dMin);

		// Find intersection segments
		std::vector<std::pair<glm::vec3, glm::vec3>> segments;
		for (size_t i = 0; i < indices.size(); i += 3) {
			glm::vec3 p[3] =
				{vertices[indices[i]].Position, vertices[indices[i + 1]].Position, vertices[indices[i + 2]].Position};
			float h[3];
			for (int j = 0; j < 3; ++j)
				h[j] = glm::dot(n, p[j]) - dTarget;

			std::vector<glm::vec3> intersections;
			for (int j = 0; j < 3; ++j) {
				int next = (j + 1) % 3;
				// Check if the edge crosses or touches the plane
				if ((h[j] <= 0 && h[next] > 0) || (h[j] > 0 && h[next] <= 0)) {
					float t = -h[j] / (h[next] - h[j]);
					intersections.push_back(p[j] + t * (p[next] - p[j]));
				}
			}

			// We only care about intersections that form a segment within this triangle
			if (intersections.size() == 2) {
				segments.push_back({intersections[0], intersections[1]});
			} else if (intersections.size() == 3) {
				// This happens if the triangle lies exactly on the plane
				segments.push_back({intersections[0], intersections[1]});
				segments.push_back({intersections[1], intersections[2]});
				segments.push_back({intersections[2], intersections[0]});
			}
		}

		if (segments.empty())
			return slice;

		// Calculate centroid of all intersection points
		glm::vec3 centroid(0.0f);
		for (const auto& s : segments) {
			centroid += s.first + s.second;
		}
		centroid /= static_cast<float>(segments.size() * 2);

		// Create triangle soup by fanning from the centroid to each intersection segment
		for (const auto& s : segments) {
			slice.triangles.push_back(centroid);
			slice.triangles.push_back(s.first);
			slice.triangles.push_back(s.second);

			// Calculate area of this triangle
			glm::vec3 a = centroid;
			glm::vec3 b = s.first;
			glm::vec3 c = s.second;
			slice.area += 0.5f * glm::length(glm::cross(b - a, c - a));
		}

		return slice;
	}

} // namespace Boidsish
