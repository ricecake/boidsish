#include "instance_manager.h"

#include <set>

#include "constants.h"
#include "dot.h"
#include "logger.h"
#include "model.h"
#include "profiling.h"
#include <GL/glew.h>

namespace Boidsish {

	// Indirect draw command for glDrawElementsIndirect
	struct DrawElementsIndirectCommand {
		GLuint count;
		GLuint instanceCount;
		GLuint firstIndex;
		GLint  baseVertex;
		GLuint baseInstance;
	};

	// Helper to validate VAO ID
	static bool IsValidVAO(GLuint vao) {
		return vao != 0 && glIsVertexArray(vao) == GL_TRUE;
	}

	InstanceManager::InstanceManager() {}

	InstanceManager::~InstanceManager() {
		for (auto& [key, group] : m_instance_groups) {
			if (group.instance_matrix_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_matrix_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_matrix_ssbo_);
			}
			if (group.instance_color_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_color_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_color_ssbo_);
			}
			if (group.visible_indices_ssbo_ != 0) {
				glDeleteBuffers(1, &group.visible_indices_ssbo_);
			}
			if (group.indirect_draw_buffer_ != 0) {
				glDeleteBuffers(1, &group.indirect_draw_buffer_);
			}
		}
	}

	void InstanceManager::InitializeCompute() {
		if (m_cull_shader)
			return;
		m_cull_shader = std::make_unique<ComputeShader>("shaders/cull.comp");
		if (m_cull_shader->isValid()) {
			GLuint frustum_idx = glGetUniformBlockIndex(m_cull_shader->ID, "FrustumData");
			if (frustum_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(m_cull_shader->ID, frustum_idx, Constants::UboBinding::FrustumData());
			}
		}
	}

	void InstanceManager::AddInstance(std::shared_ptr<Shape> shape) {
		// Group by instance key - same key means shapes can be instanced together
		m_instance_groups[shape->GetInstanceKey()].shapes.push_back(shape);
	}

	void InstanceManager::Render(Shader& shader) {
		shader.use();
		shader.setBool("is_instanced", true);

		for (auto& [key, group] : m_instance_groups) {
			if (group.shapes.empty()) {
				continue;
			}

			// Determine type from the key prefix
			if (key.starts_with("Model:")) {
				RenderModelGroup(shader, group);
			} else if (key == "Dot") {
				RenderDotGroup(shader, group);
			}
			// Other shape types can be added here
			group.shapes.clear();
		}

		// Reset all shader uniforms to safe defaults after all groups rendered
		shader.setBool("is_instanced", false);
		shader.setBool("useInstanceColor", false);
		shader.setBool("useSSBOInstancing", false);
		shader.setBool("useIndirectRendering", false);
		shader.setBool("isColossal", false);
		shader.setBool("usePBR", false);
		shader.setBool("isTextEffect", false);
		shader.setFloat("objectAlpha", 1.0f);

		// Clean up buffer state
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		PROJECT_PROFILE_SCOPE("InstanceManager::RenderModelGroup");
		InitializeCompute();

		// Reset shader state at start of each model group to prevent state leakage
		shader.setBool("useInstanceColor", false);
		shader.setBool("usePBR", false);

		// Ensure buffer capacities and mapping (AZDO)
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		if (group.instance_matrix_ssbo_ == 0 || group.shapes.size() > group.matrix_capacity_) {
			if (group.instance_matrix_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_matrix_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_matrix_ssbo_);
			}
			glGenBuffers(1, &group.instance_matrix_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_matrix_ssbo_);
			group.matrix_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, group.matrix_capacity_ * sizeof(glm::mat4), nullptr, flags);
			group.matrix_ptr_ = (glm::mat4*)glMapBufferRange(
				GL_SHADER_STORAGE_BUFFER,
				0,
				group.matrix_capacity_ * sizeof(glm::mat4),
				flags
			);
		}

		if (group.instance_color_ssbo_ == 0 || group.shapes.size() > group.color_capacity_) {
			if (group.instance_color_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_color_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_color_ssbo_);
			}
			glGenBuffers(1, &group.instance_color_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_color_ssbo_);
			group.color_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, group.color_capacity_ * sizeof(glm::vec4), nullptr, flags);
			group.color_ptr_ = (glm::vec4*)glMapBufferRange(
				GL_SHADER_STORAGE_BUFFER,
				0,
				group.color_capacity_ * sizeof(glm::vec4),
				flags
			);
		}

		if (group.visible_indices_ssbo_ == 0 || group.shapes.size() > group.visible_capacity_) {
			if (group.visible_indices_ssbo_ != 0)
				glDeleteBuffers(1, &group.visible_indices_ssbo_);
			glGenBuffers(1, &group.visible_indices_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.visible_indices_ssbo_);
			group.visible_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferData(GL_SHADER_STORAGE_BUFFER, group.visible_capacity_ * sizeof(GLuint), nullptr, GL_STREAM_DRAW);
		}

		if (group.indirect_draw_buffer_ == 0) {
			glGenBuffers(1, &group.indirect_draw_buffer_);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);
			glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand), nullptr, GL_STREAM_DRAW);
		}

		// Populate matrices and colors
		for (size_t i = 0; i < group.shapes.size(); ++i) {
			group.matrix_ptr_[i] = group.shapes[i]->GetModelMatrix();
			group.color_ptr_[i] =
				glm::vec4(group.shapes[i]->GetR(), group.shapes[i]->GetG(), group.shapes[i]->GetB(), group.shapes[i]->GetA());
		}

		// GPU Frustum Culling Pass
		m_cull_shader->use();
		m_cull_shader->setUint("numInstances", (unsigned int)group.shapes.size());
		m_cull_shader->setFloat("boundingRadius", group.shapes[0]->GetBoundingRadius());

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, group.instance_matrix_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, group.visible_indices_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, group.indirect_draw_buffer_);

		// Get the Model from the first shape to access mesh data
		auto model = std::dynamic_pointer_cast<Model>(group.shapes[0]);
		if (!model)
			return;

		shader.setBool("isColossal", model->IsColossal());
		shader.setFloat("objectAlpha", model->GetA());
		shader.setBool("useSSBOInstancing", true);
		shader.setBool("useIndirectRendering", true);

		for (const auto& mesh : model->getMeshes()) {
			if (!IsValidVAO(mesh.VAO))
				continue;

			// Reset indirect command for this mesh
			DrawElementsIndirectCommand cmd;
			cmd.count = (GLuint)mesh.indices.size();
			cmd.instanceCount = 0; // Will be incremented by compute shader
			cmd.firstIndex = 0;
			cmd.baseVertex = 0;
			cmd.baseInstance = 0;
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), &cmd);

			// Dispatch culling
			unsigned int numGroups = ((unsigned int)group.shapes.size() + 63) / 64;
			m_cull_shader->use(); // Ensure cull shader is active
			glDispatchCompute(numGroups, 1, 1);
			glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Render
			shader.use();
			mesh.bindTextures(shader);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, group.instance_matrix_ssbo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, group.visible_indices_ssbo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, group.instance_color_ssbo_);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);

			mesh.render_indirect(true);
		}

		shader.setBool("useSSBOInstancing", false);
		shader.setBool("useIndirectRendering", false);
	}

	void InstanceManager::RenderDotGroup(Shader& shader, InstanceGroup& group) {
		PROJECT_PROFILE_SCOPE("InstanceManager::RenderDotGroup");
		InitializeCompute();

		// Ensure sphere VAO is initialized and valid before rendering
		if (!IsValidVAO(Shape::sphere_vao_)) {
			logger::ERROR("Invalid sphere VAO {} - skipping dot rendering", Shape::sphere_vao_);
			return;
		}

		// Reset shader state at start to prevent leakage from previous model groups
		shader.setBool("isColossal", false);
		shader.setFloat("objectAlpha", 1.0f);

		// Clear any stale texture bindings from model rendering
		for (unsigned int i = 0; i < 4; i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		glActiveTexture(GL_TEXTURE0);

		// Ensure capacities and mapping (AZDO)
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

		if (group.instance_matrix_ssbo_ == 0 || group.shapes.size() > group.matrix_capacity_) {
			if (group.instance_matrix_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_matrix_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_matrix_ssbo_);
			}
			glGenBuffers(1, &group.instance_matrix_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_matrix_ssbo_);
			group.matrix_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, group.matrix_capacity_ * sizeof(glm::mat4), nullptr, flags);
			group.matrix_ptr_ = (glm::mat4*)glMapBufferRange(
				GL_SHADER_STORAGE_BUFFER,
				0,
				group.matrix_capacity_ * sizeof(glm::mat4),
				flags
			);
		}

		if (group.instance_color_ssbo_ == 0 || group.shapes.size() > group.color_capacity_) {
			if (group.instance_color_ssbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_color_ssbo_);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glDeleteBuffers(1, &group.instance_color_ssbo_);
			}
			glGenBuffers(1, &group.instance_color_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.instance_color_ssbo_);
			group.color_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, group.color_capacity_ * sizeof(glm::vec4), nullptr, flags);
			group.color_ptr_ = (glm::vec4*)glMapBufferRange(
				GL_SHADER_STORAGE_BUFFER,
				0,
				group.color_capacity_ * sizeof(glm::vec4),
				flags
			);
		}

		if (group.visible_indices_ssbo_ == 0 || group.shapes.size() > group.visible_capacity_) {
			if (group.visible_indices_ssbo_ != 0)
				glDeleteBuffers(1, &group.visible_indices_ssbo_);
			glGenBuffers(1, &group.visible_indices_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.visible_indices_ssbo_);
			group.visible_capacity_ = std::max(group.shapes.size(), (size_t)128);
			glBufferData(GL_SHADER_STORAGE_BUFFER, group.visible_capacity_ * sizeof(GLuint), nullptr, GL_STREAM_DRAW);
		}

		if (group.indirect_draw_buffer_ == 0) {
			glGenBuffers(1, &group.indirect_draw_buffer_);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);
			glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand), nullptr, GL_STREAM_DRAW);
		}

		// Populate matrices and colors
		for (size_t i = 0; i < group.shapes.size(); ++i) {
			group.matrix_ptr_[i] = group.shapes[i]->GetModelMatrix();
			group.color_ptr_[i] =
				glm::vec4(group.shapes[i]->GetR(), group.shapes[i]->GetG(), group.shapes[i]->GetB(), group.shapes[i]->GetA());
		}

		// GPU Frustum Culling Pass
		m_cull_shader->use();
		m_cull_shader->setUint("numInstances", (unsigned int)group.shapes.size());
		m_cull_shader->setFloat("boundingRadius", group.shapes[0]->GetBoundingRadius());

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, group.instance_matrix_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, group.visible_indices_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, group.indirect_draw_buffer_);

		// Reset indirect command
		DrawElementsIndirectCommand cmd;
		cmd.count = Shape::sphere_vertex_count_;
		cmd.instanceCount = 0;
		cmd.firstIndex = 0;
		cmd.baseVertex = 0;
		cmd.baseInstance = 0;
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);
		glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), &cmd);

		unsigned int numGroups = ((unsigned int)group.shapes.size() + 63) / 64;
		glDispatchCompute(numGroups, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		// Get first Dot's properties for shared settings
		auto dot = std::dynamic_pointer_cast<Dot>(group.shapes[0]);
		if (!dot)
			return;

		shader.use();
		shader.setBool("isColossal", false);
		shader.setBool("useInstanceColor", true);
		shader.setBool("useSSBOInstancing", true);
		shader.setBool("useIndirectRendering", true);

		// Set PBR properties
		shader.setBool("usePBR", dot->UsePBR());
		if (dot->UsePBR()) {
			shader.setFloat("roughness", dot->GetRoughness());
			shader.setFloat("metallic", dot->GetMetallic());
			shader.setFloat("ao", dot->GetAO());
		}

		// Use the shared sphere VAO
		glBindVertexArray(Shape::sphere_vao_);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, group.instance_matrix_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, group.visible_indices_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, group.instance_color_ssbo_);

		// Draw
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_draw_buffer_);
		glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
		shader.setBool("useInstanceColor", false);
		shader.setBool("useSSBOInstancing", false);
		shader.setBool("useIndirectRendering", false);
	}

} // namespace Boidsish
