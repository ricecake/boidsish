#include "instance_manager.h"

#include <set>

#include "asset_manager.h"
#include "dot.h"
#include "logger.h"
#include "model.h"
#include "profiler.h"
#include <GL/glew.h>

namespace Boidsish {

	struct DrawElementsIndirectCommand {
		unsigned int count;
		unsigned int instanceCount;
		unsigned int firstIndex;
		unsigned int baseVertex;
		unsigned int baseInstance;
	};

	// Helper to validate VAO ID
	static bool IsValidVAO(GLuint vao) {
		return vao != 0 && glIsVertexArray(vao) == GL_TRUE;
	}

	InstanceManager::InstanceManager() {}

	InstanceManager::~InstanceManager() {
		for (auto& [key, group] : m_instance_groups) {
			if (group.indirect_buffer)
				glDeleteBuffers(1, &group.indirect_buffer);
			if (group.atomic_counter_buffer)
				glDeleteBuffers(1, &group.atomic_counter_buffer);
			if (group.handle_ssbo)
				glDeleteBuffers(1, &group.handle_ssbo);
		}
		m_instance_groups.clear();
	}

	void InstanceManager::_InitializeShaders() {
		if (m_culling_shader)
			return;

		m_culling_shader = std::make_unique<ComputeShader>("shaders/instance_culling.comp");
		m_update_commands_shader = std::make_unique<ComputeShader>("shaders/instance_update_commands.comp");
	}

	void InstanceManager::AddInstance(std::shared_ptr<Shape> shape) {
		// Group by instance key - same key means shapes can be instanced together
		m_instance_groups[shape->GetInstanceKey()].shapes.push_back(shape);
	}

	void InstanceManager::Render(Shader& shader) {
		PROJECT_PROFILE_SCOPE("InstanceManager::Render");
		shader.use();
		shader.setBool("is_instanced", true);

		for (auto& [key, group] : m_instance_groups) {
			if (group.shapes.empty()) {
				continue;
			}

			PROJECT_PROFILE_SCOPE(key.c_str());
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
		shader.setBool("isColossal", false);
		shader.setBool("usePBR", false);
		shader.setBool("isTextEffect", false);
		shader.setFloat("objectAlpha", 1.0f);

		// Clean up buffer state
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		PROJECT_PROFILE_SCOPE("RenderModelGroup");
		_InitializeShaders();

		// Reset shader state at start of each model group to prevent state leakage
		shader.setBool("useInstanceColor", false);
		shader.setBool("usePBR", false);

		// Get the Model from the first shape to access mesh data
		auto model = std::dynamic_pointer_cast<Model>(group.shapes[0]);
		if (!model) {
			return;
		}

		// Create/update matrix buffers
		if (!group.instance_matrix_buffer) {
			group.instance_matrix_buffer =
				std::make_unique<PersistentBuffer<glm::mat4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
			group.visible_matrix_buffer =
				std::make_unique<PersistentBuffer<glm::mat4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
		} else {
			group.instance_matrix_buffer->ensureCapacity(group.shapes.size());
			group.visible_matrix_buffer->ensureCapacity(group.shapes.size());
		}

		// Build instance matrices directly into persistent buffer
		for (size_t i = 0; i < group.shapes.size(); ++i) {
			(*group.instance_matrix_buffer)[i] = group.shapes[i]->GetModelMatrix();
		}

		auto model_data = AssetManager::GetInstance().GetModelData(model->GetModelPath());
		if (!model_data || !model_data->has_unified) {
			return;
		}

		// Prepare MDI commands and bindless textures
		std::vector<DrawElementsIndirectCommand> commands;
		std::vector<uint64_t>                    diffuse_handles;
		bool                                     any_texture = false;
		unsigned int                             index_offset = 0;
		unsigned int                             vertex_offset = 0;

		for (const auto& mesh : model_data->meshes) {
			DrawElementsIndirectCommand cmd;
			cmd.count = (unsigned int)mesh.indices.size();
			cmd.instanceCount = 0; // Filled by GPU
			cmd.firstIndex = index_offset;
			cmd.baseVertex = vertex_offset;
			cmd.baseInstance = 0;
			commands.push_back(cmd);

			index_offset += (unsigned int)mesh.indices.size();
			vertex_offset += (unsigned int)mesh.vertices.size();

			uint64_t handle = 0;
			if (GLEW_ARB_bindless_texture) {
				for (const auto& tex : mesh.textures) {
					if (tex.type == "texture_diffuse") {
						handle = tex.handle;
						any_texture = true;
						break;
					}
				}
			}
			diffuse_handles.push_back(handle);
		}

		// Ensure indirect and atomic buffers are ready
		if (group.indirect_buffer == 0) {
			glGenBuffers(1, &group.indirect_buffer);
			glGenBuffers(1, &group.atomic_counter_buffer);
		}

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_buffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, commands.size() * sizeof(DrawElementsIndirectCommand), commands.data(), GL_STREAM_DRAW);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, group.atomic_counter_buffer);
		unsigned int zero = 0;
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), &zero, GL_STREAM_DRAW);

		// 1. GPU Culling
		m_culling_shader->use();
		m_culling_shader->setInt("u_numInstances", (int)group.shapes.size());
		m_culling_shader->setFloat("u_radius", model->GetBoundingRadius());
		m_culling_shader->setBool("u_hasColors", false);
		group.instance_matrix_buffer->bindBase(0);
		group.visible_matrix_buffer->bindBase(1);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, group.atomic_counter_buffer);
		m_culling_shader->dispatch((group.shapes.size() + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		// 2. Update indirect commands
		m_update_commands_shader->use();
		m_update_commands_shader->setInt("u_numCommands", (int)commands.size());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, group.indirect_buffer);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, group.atomic_counter_buffer);
		m_update_commands_shader->dispatch(1, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		// 3. Rendering
		shader.use();
		shader.setBool("isColossal", model->IsColossal());
		shader.setFloat("objectAlpha", model->GetA());
		shader.setBool("useInstanceColor", false);
		shader.setBool("use_texture", any_texture);
		shader.setBool("u_useBindless", GLEW_ARB_bindless_texture && any_texture);
		shader.setBool("u_useMDI", true);
		shader.setBool("useSSBOInstancing", true);

		if (any_texture) {
			if (group.handle_ssbo == 0) {
				glGenBuffers(1, &group.handle_ssbo);
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, group.handle_ssbo);
			glBufferData(GL_SHADER_STORAGE_BUFFER, diffuse_handles.size() * sizeof(uint64_t), diffuse_handles.data(), GL_STREAM_DRAW);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, group.handle_ssbo);
		}

		group.visible_matrix_buffer->bindBase(10);
		glBindVertexArray(model_data->unified_vao);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_buffer);

		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, (GLsizei)commands.size(), 0);

		// Cleanup
		glBindVertexArray(0);
		shader.setBool("u_useMDI", false);
		shader.setBool("useSSBOInstancing", false);
	}

	void InstanceManager::RenderDotGroup(Shader& shader, InstanceGroup& group) {
		PROJECT_PROFILE_SCOPE("RenderDotGroup");
		_InitializeShaders();

		// Ensure sphere VAO is initialized and valid before rendering
		if (!IsValidVAO(Shape::sphere_vao_)) {
			logger::ERROR("Invalid sphere VAO {} - skipping dot rendering", Shape::sphere_vao_);
			return;
		}

		// Reset shader state at start to prevent leakage from previous model groups
		shader.setBool("isColossal", false);
		shader.setFloat("objectAlpha", 1.0f);

		// Create/update matrix buffers
		if (!group.instance_matrix_buffer) {
			group.instance_matrix_buffer =
				std::make_unique<PersistentBuffer<glm::mat4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
			group.visible_matrix_buffer =
				std::make_unique<PersistentBuffer<glm::mat4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
			group.instance_color_buffer =
				std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
			group.visible_color_buffer =
				std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, group.shapes.size());
		} else {
			group.instance_matrix_buffer->ensureCapacity(group.shapes.size());
			group.visible_matrix_buffer->ensureCapacity(group.shapes.size());
			group.instance_color_buffer->ensureCapacity(group.shapes.size());
			group.visible_color_buffer->ensureCapacity(group.shapes.size());
		}

		// Build instance matrices and colors directly into persistent buffers
		for (size_t i = 0; i < group.shapes.size(); ++i) {
			(*group.instance_matrix_buffer)[i] = group.shapes[i]->GetModelMatrix();
			(*group.instance_color_buffer)[i] =
				glm::vec4(group.shapes[i]->GetR(), group.shapes[i]->GetG(), group.shapes[i]->GetB(), group.shapes[i]->GetA());
		}

		// Ensure indirect and atomic buffers are ready
		if (group.indirect_buffer == 0) {
			glGenBuffers(1, &group.indirect_buffer);
			glGenBuffers(1, &group.atomic_counter_buffer);
		}

		DrawElementsIndirectCommand cmd;
		cmd.count = (unsigned int)Shape::sphere_vertex_count_;
		cmd.instanceCount = 0;
		cmd.firstIndex = 0;
		cmd.baseVertex = 0;
		cmd.baseInstance = 0;

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_buffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand), &cmd, GL_STREAM_DRAW);

		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, group.atomic_counter_buffer);
		unsigned int zero = 0;
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), &zero, GL_STREAM_DRAW);

		// 1. GPU Culling
		m_culling_shader->use();
		m_culling_shader->setInt("u_numInstances", (int)group.shapes.size());
		m_culling_shader->setFloat("u_radius", group.shapes[0]->GetBoundingRadius());
		m_culling_shader->setBool("u_hasColors", true);
		group.instance_matrix_buffer->bindBase(0);
		group.visible_matrix_buffer->bindBase(1);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, group.atomic_counter_buffer);
		group.instance_color_buffer->bindBase(4);
		group.visible_color_buffer->bindBase(5);
		m_culling_shader->dispatch((group.shapes.size() + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		// 2. Update indirect command
		m_update_commands_shader->use();
		m_update_commands_shader->setInt("u_numCommands", 1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, group.indirect_buffer);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, group.atomic_counter_buffer);
		m_update_commands_shader->dispatch(1, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		// 3. Rendering
		auto dot = std::dynamic_pointer_cast<Dot>(group.shapes[0]);
		shader.use();
		shader.setBool("isColossal", false);
		shader.setBool("useInstanceColor", true);
		shader.setBool("usePBR", dot->UsePBR());
		if (dot->UsePBR()) {
			shader.setFloat("roughness", dot->GetRoughness());
			shader.setFloat("metallic", dot->GetMetallic());
			shader.setFloat("ao", dot->GetAO());
		}
		shader.setBool("useSSBOInstancing", true);

		group.visible_matrix_buffer->bindBase(10);

		glBindVertexArray(Shape::sphere_vao_);
		glBindBuffer(GL_ARRAY_BUFFER, group.visible_matrix_buffer->id());
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);

		glBindBuffer(GL_ARRAY_BUFFER, group.visible_color_buffer->id());
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glVertexAttribDivisor(7, 1);

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, group.indirect_buffer);
		glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);

		// Cleanup
		glBindVertexArray(0);
		shader.setBool("useSSBOInstancing", false);
		shader.setBool("useInstanceColor", false);
	}

} // namespace Boidsish
