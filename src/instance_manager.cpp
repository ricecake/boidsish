#include "instance_manager.h"

#include "model.h"
#include <GL/glew.h>

namespace Boidsish {

	void InstanceManager::AddInstance(std::shared_ptr<Shape> shape) {
		m_instance_groups[std::type_index(typeid(*shape))].shapes.push_back(shape);
	}

	void InstanceManager::Render(Shader& shader) {
		shader.use();
		shader.setBool("is_instanced", true);

		for (auto& pair : m_instance_groups) {
			auto& group = pair.second;
			if (group.shapes.empty()) {
				continue;
			}

			std::vector<glm::mat4> model_matrices;
			model_matrices.reserve(group.shapes.size());
			for (const auto& shape : group.shapes) {
				model_matrices.push_back(shape->GetModelMatrix());
			}

			if (group.instance_vbo_ == 0) {
				glGenBuffers(1, &group.instance_vbo_);
			}

			glBindBuffer(GL_ARRAY_BUFFER, group.instance_vbo_);
			if (model_matrices.size() > group.capacity_) {
				glBufferData(
					GL_ARRAY_BUFFER,
					model_matrices.size() * sizeof(glm::mat4),
					&model_matrices[0],
					GL_DYNAMIC_DRAW
				);
				group.capacity_ = model_matrices.size();
			} else {
				glBufferSubData(GL_ARRAY_BUFFER, 0, model_matrices.size() * sizeof(glm::mat4), &model_matrices[0]);
			}

			auto model = std::dynamic_pointer_cast<Model>(group.shapes[0]);
			if (model) {
				shader.setBool("isColossal", model->IsColossal());
				for (const auto& mesh : model->getMeshes()) {
					mesh.bindTextures(shader);

					glBindVertexArray(mesh.VAO);
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

					mesh.render_instanced(group.shapes.size());

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
			}
		}
		m_instance_groups.clear();
		shader.setBool("is_instanced", false);
	}

} // namespace Boidsish
