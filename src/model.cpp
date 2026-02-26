#include "model.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "animator.h"
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

		if (m_animator && m_animator->GetCurrentAnimationIndex() != -1) {
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
			packet.uniforms.use_texture = !mesh.textures.empty();
			packet.uniforms.is_colossal = IsColossal();
			packet.uniforms.use_vertex_color = 1;

			packet.uniforms.dissolve_enabled = dissolve_enabled_ ? 1 : 0;
			packet.uniforms.dissolve_plane_normal = dissolve_plane_normal_;
			packet.uniforms.dissolve_plane_dist = actual_dissolve_dist;

			if (m_animator && m_animator->GetCurrentAnimationIndex() != -1) {
				packet.uniforms.use_skinning = 1;
				packet.bone_matrices = m_animator->GetFinalBoneMatrices();
			}

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
		}
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
