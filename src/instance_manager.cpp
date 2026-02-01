#include "instance_manager.h"

#include "dot.h"
#include "model.h"
#include "profiler.h"
#include "opengl_helpers.h"
#include <GL/glew.h>
#include <cstring>
#include <algorithm>

namespace Boidsish {

	struct DrawElementsIndirectCommand {
		unsigned int count;
		unsigned int instanceCount;
		unsigned int firstIndex;
		int          baseVertex;
		unsigned int baseInstance;
	};

	InstanceManager::~InstanceManager() {
		if (m_indirect_buffer != 0) {
			glDeleteBuffers(1, &m_indirect_buffer);
		}
		m_instance_groups.clear(); // ring buffers reset automatically
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

			// Determine type from the key prefix
			if (key.starts_with("Model:")) {
				RenderModelGroup(shader, group);
			} else if (key == "Dot") {
				RenderDotGroup(shader, group);
			}

			if (group.matrix_ring) group.matrix_ring->AdvanceFrame();
			if (group.color_ring) group.color_ring->AdvanceFrame();

			group.shapes.clear();
		}

		shader.setBool("is_instanced", false);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		// Build instance matrices
		std::vector<glm::mat4> model_matrices;
		model_matrices.reserve(group.shapes.size());
		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());
		}

		if (!group.matrix_ring) {
			group.matrix_ring = std::make_unique<PersistentRingBuffer>(GL_ARRAY_BUFFER, std::max(static_cast<size_t>(100), model_matrices.size()) * sizeof(glm::mat4));
		}

		group.matrix_ring->EnsureCapacity(model_matrices.size() * sizeof(glm::mat4));
		void* ptr = group.matrix_ring->GetCurrentPtr();
		if (ptr && !model_matrices.empty()) {
			memcpy(ptr, model_matrices.data(), model_matrices.size() * sizeof(glm::mat4));
		}

		// Get the Model from the first shape to access mesh data
		auto model = std::dynamic_pointer_cast<Model>(group.shapes[0]);
		if (!model) {
			return;
		}

		shader.setBool("isColossal", model->IsColossal());
		shader.setFloat("objectAlpha", model->GetA());
		shader.setBool("useInstanceColor", false);

		unsigned int modelVAO = model->GetVAO();
		if (modelVAO == 0) return;

		glBindVertexArray(modelVAO);

		// Setup instance matrix attribute (location 3-6)
		glBindBuffer(GL_ARRAY_BUFFER, group.matrix_ring->GetVBO());
		size_t base_offset = group.matrix_ring->GetOffset();

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)base_offset);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(base_offset + sizeof(glm::vec4)));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(base_offset + 2 * sizeof(glm::vec4)));
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(base_offset + 3 * sizeof(glm::vec4)));

		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);

		const auto& counts = model->GetMeshIndicesCount();
		const auto& offsets = model->GetMeshIndicesOffset();
		const auto& base_verts = model->GetMeshVerticesOffset();

		if (glewIsSupported("GL_ARB_multi_draw_indirect")) {
			std::vector<DrawElementsIndirectCommand> commands;
			commands.reserve(counts.size());

			for (size_t i = 0; i < counts.size(); ++i) {
				DrawElementsIndirectCommand cmd;
				cmd.count = counts[i];
				cmd.instanceCount = group.shapes.size();
				cmd.firstIndex = offsets[i];
				cmd.baseVertex = base_verts[i];
				cmd.baseInstance = 0;
				commands.push_back(cmd);
			}

			if (m_indirect_buffer == 0) {
				glGenBuffers(1, &m_indirect_buffer);
			}
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirect_buffer);
			glBufferData(GL_DRAW_INDIRECT_BUFFER, commands.size() * sizeof(DrawElementsIndirectCommand), commands.data(), GL_STREAM_DRAW);

			model->getMeshes()[0].bindTextures(shader);

			glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, commands.size(), 0);
		} else {
			// Fallback to regular instanced rendering
			for (size_t i = 0; i < model->getMeshes().size(); ++i) {
				model->getMeshes()[i].bindTextures(shader);
				glDrawElementsInstancedBaseVertex(GL_TRIANGLES, counts[i], GL_UNSIGNED_INT, (void*)(uintptr_t)(offsets[i] * sizeof(unsigned int)), group.shapes.size(), base_verts[i]);
			}
		}

		glVertexAttribDivisor(3, 0);
		glVertexAttribDivisor(4, 0);
		glVertexAttribDivisor(5, 0);
		glVertexAttribDivisor(6, 0);

		glDisableVertexAttribArray(3);
		glDisableVertexAttribArray(4);
		glDisableVertexAttribArray(5);
		glDisableVertexAttribArray(6);
		glBindVertexArray(0);
	}

	void InstanceManager::RenderDotGroup(Shader& shader, InstanceGroup& group) {
		// Ensure sphere VAO is initialized before rendering
		if (Shape::sphere_vao_ == 0)
			return;

		// Build instance matrices and colors
		std::vector<glm::mat4> model_matrices;
		std::vector<glm::vec4> colors;
		model_matrices.reserve(group.shapes.size());
		colors.reserve(group.shapes.size());

		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());
			colors.emplace_back(shape->GetR(), shape->GetG(), shape->GetB(), shape->GetA());
		}

		if (!group.matrix_ring) {
			group.matrix_ring = std::make_unique<PersistentRingBuffer>(GL_ARRAY_BUFFER, std::max(static_cast<size_t>(100), model_matrices.size()) * sizeof(glm::mat4));
		}
		group.matrix_ring->EnsureCapacity(model_matrices.size() * sizeof(glm::mat4));
		void* m_ptr = group.matrix_ring->GetCurrentPtr();
		if (m_ptr && !model_matrices.empty()) {
			memcpy(m_ptr, model_matrices.data(), model_matrices.size() * sizeof(glm::mat4));
		}

		if (!group.color_ring) {
			group.color_ring = std::make_unique<PersistentRingBuffer>(GL_ARRAY_BUFFER, std::max(static_cast<size_t>(100), colors.size()) * sizeof(glm::vec4));
		}
		group.color_ring->EnsureCapacity(colors.size() * sizeof(glm::vec4));
		void* c_ptr = group.color_ring->GetCurrentPtr();
		if (c_ptr && !colors.empty()) {
			memcpy(c_ptr, colors.data(), colors.size() * sizeof(glm::vec4));
		}

		// Get first Dot's properties for shared settings
		auto dot = std::dynamic_pointer_cast<Dot>(group.shapes[0]);
		if (!dot) {
			return;
		}

		shader.setBool("isColossal", false);
		shader.setBool("useInstanceColor", true);

		// Set PBR properties (using first dot's values - could be extended to per-instance)
		shader.setBool("usePBR", dot->UsePBR());
		if (dot->UsePBR()) {
			shader.setFloat("roughness", dot->GetRoughness());
			shader.setFloat("metallic", dot->GetMetallic());
			shader.setFloat("ao", dot->GetAO());
		}

		// Use the shared sphere VAO
		glBindVertexArray(Shape::sphere_vao_);

		// Setup instance matrix attribute (location 3-6)
		glBindBuffer(GL_ARRAY_BUFFER, group.matrix_ring->GetVBO());
		size_t m_offset = group.matrix_ring->GetOffset();

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)m_offset);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(m_offset + sizeof(glm::vec4)));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(m_offset + 2 * sizeof(glm::vec4)));
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(m_offset + 3 * sizeof(glm::vec4)));

		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);

		// Setup instance color attribute (location 7)
		glBindBuffer(GL_ARRAY_BUFFER, group.color_ring->GetVBO());
		size_t c_offset = group.color_ring->GetOffset();
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)c_offset);
		glVertexAttribDivisor(7, 1);

		// Draw instanced spheres
		glDrawElementsInstanced(GL_TRIANGLES, Shape::sphere_vertex_count_, GL_UNSIGNED_INT, 0, group.shapes.size());

		// Reset divisors
		glVertexAttribDivisor(3, 0);
		glVertexAttribDivisor(4, 0);
		glVertexAttribDivisor(5, 0);
		glVertexAttribDivisor(6, 0);
		glVertexAttribDivisor(7, 0);

		glDisableVertexAttribArray(3);
		glDisableVertexAttribArray(4);
		glDisableVertexAttribArray(5);
		glDisableVertexAttribArray(6);
		glDisableVertexAttribArray(7);

		glBindVertexArray(0);
		shader.setBool("useInstanceColor", false);
	}

} // namespace Boidsish
