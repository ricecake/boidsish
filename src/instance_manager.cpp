#include "instance_manager.h"

#include "dot.h"
#include "model.h"
#include <GL/glew.h>

namespace Boidsish {

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
		}

		m_instance_groups.clear();
		shader.setBool("is_instanced", false);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		// Build instance matrices
		std::vector<glm::mat4> model_matrices;
		model_matrices.reserve(group.shapes.size());
		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());
		}

		// Create/update matrix VBO
		if (group.instance_matrix_vbo_ == 0) {
			glGenBuffers(1, &group.instance_matrix_vbo_);
		}

		glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
		if (model_matrices.size() > group.matrix_capacity_) {
			glBufferData(
				GL_ARRAY_BUFFER,
				model_matrices.size() * sizeof(glm::mat4),
				&model_matrices[0],
				GL_DYNAMIC_DRAW
			);
			group.matrix_capacity_ = model_matrices.size();
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, model_matrices.size() * sizeof(glm::mat4), &model_matrices[0]);
		}

		// Get the Model from the first shape to access mesh data
		auto model = std::dynamic_pointer_cast<Model>(group.shapes[0]);
		if (!model) {
			return;
		}

		shader.setBool("isColossal", model->IsColossal());
		shader.setFloat("objectAlpha", model->GetA());
		shader.setBool("useInstanceColor", false); // Models use textures, not per-instance colors

		for (const auto& mesh : model->getMeshes()) {
			// Bind textures
			unsigned int diffuseNr = 1;
			unsigned int specularNr = 1;
			for (unsigned int i = 0; i < mesh.textures.size(); i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				std::string number;
				std::string name = mesh.textures[i].type;
				if (name == "texture_diffuse")
					number = std::to_string(diffuseNr++);
				else if (name == "texture_specular")
					number = std::to_string(specularNr++);
				shader.setInt(("material." + name + number).c_str(), i);
				glBindTexture(GL_TEXTURE_2D, mesh.textures[i].id);
			}
			glActiveTexture(GL_TEXTURE0);

			glBindVertexArray(mesh.VAO);

			// Setup instance matrix attribute (location 3-6, mat4 takes 4 vec4 slots)
			glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
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

	void InstanceManager::RenderDotGroup(Shader& shader, InstanceGroup& group) {
		// Build instance matrices and colors
		std::vector<glm::mat4> model_matrices;
		std::vector<glm::vec4> colors;
		model_matrices.reserve(group.shapes.size());
		colors.reserve(group.shapes.size());

		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());
			colors.emplace_back(shape->GetR(), shape->GetG(), shape->GetB(), shape->GetA());
		}

		// Create/update matrix VBO
		if (group.instance_matrix_vbo_ == 0) {
			glGenBuffers(1, &group.instance_matrix_vbo_);
		}

		glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
		if (model_matrices.size() > group.matrix_capacity_) {
			glBufferData(
				GL_ARRAY_BUFFER,
				model_matrices.size() * sizeof(glm::mat4),
				&model_matrices[0],
				GL_DYNAMIC_DRAW
			);
			group.matrix_capacity_ = model_matrices.size();
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, model_matrices.size() * sizeof(glm::mat4), &model_matrices[0]);
		}

		// Create/update color VBO
		if (group.instance_color_vbo_ == 0) {
			glGenBuffers(1, &group.instance_color_vbo_);
		}

		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		if (colors.size() > group.color_capacity_) {
			glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), &colors[0], GL_DYNAMIC_DRAW);
			group.color_capacity_ = colors.size();
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, colors.size() * sizeof(glm::vec4), &colors[0]);
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
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
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

		// Setup instance color attribute (location 7)
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
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
