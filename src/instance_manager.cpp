#include "instance_manager.h"

#include <set>

#include "checkpoint_ring.h"
#include "dot.h"
#include "line.h"
#include "logger.h"
#include "model.h"
#include <GL/glew.h>

namespace Boidsish {

	// Helper to validate VAO ID
	static bool IsValidVAO(GLuint vao) {
		return vao != 0 && glIsVertexArray(vao) == GL_TRUE;
	}

	InstanceManager::~InstanceManager() {
		for (auto& [key, group] : m_instance_groups) {
			if (group.instance_matrix_vbo_ != 0) {
				glDeleteBuffers(1, &group.instance_matrix_vbo_);
			}
			if (group.instance_color_vbo_ != 0) {
				glDeleteBuffers(1, &group.instance_color_vbo_);
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
			} else if (key.starts_with("Line:")) {
				RenderLineGroup(shader, group);
			} else if (key.starts_with("CheckpointRing:")) {
				RenderCheckpointRingGroup(shader, group);
			} else {
				// Fallback for unique shapes
				shader.setBool("is_instanced", false);
				for (auto& shape : group.shapes) {
					shader.setBool("isColossal", shape->IsColossal());
					shader.setFloat("frustumCullRadius", shape->GetBoundingRadius());
					shape->render(shader);
				}
				shader.setBool("is_instanced", true);
			}
			group.shapes.clear();
		}

		// Reset all shader uniforms to safe defaults after all groups rendered
		shader.setBool("is_instanced", false);
		shader.setBool("useInstanceColor", false);
		shader.setBool("isColossal", false);
		shader.setBool("usePBR", false);
		shader.setBool("isTextEffect", false);
		shader.setBool("isArcadeText", false);
		shader.setFloat("objectAlpha", 1.0f);

		// Clean up buffer state
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		// Reset shader state at start of each model group to prevent state leakage
		shader.setBool("useInstanceColor", false);
		shader.setBool("usePBR", false);

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
			glBindBuffer(GL_ARRAY_BUFFER, 0); // Clean up before early return
			return;
		}

		shader.setBool("isColossal", model->IsColossal());
		shader.setFloat("objectAlpha", model->GetA());
		shader.setBool("useInstanceColor", false); // Models use textures, not per-instance colors

		for (const auto& mesh : model->getMeshes()) {
			// Skip meshes with invalid VAO - use glIsVertexArray for thorough check
			if (!IsValidVAO(mesh.VAO)) {
				logger::ERROR("Invalid VAO {} in model {} - skipping mesh", mesh.VAO, model->GetModelPath());
				continue;
			}

			// Bind textures
			unsigned int diffuseNr = 1;
			unsigned int specularNr = 1;
			bool         hasDiffuse = false;
			for (unsigned int i = 0; i < mesh.textures.size(); i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				std::string number;
				std::string name = mesh.textures[i].type;
				if (name == "texture_diffuse") {
					number = std::to_string(diffuseNr++);
					hasDiffuse = true;
				} else if (name == "texture_specular")
					number = std::to_string(specularNr++);

				// Use correct uniform names (vis.frag doesn't use "material." prefix)
				shader.setInt((name + number).c_str(), i);
				glBindTexture(GL_TEXTURE_2D, mesh.textures[i].id);
			}
			shader.setBool("use_texture", hasDiffuse);
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

			// Tell mesh to skip its own VAO/EBO binding if we already have it correctly bound
			mesh.render_instanced(group.shapes.size(), false);

			glVertexAttribDivisor(3, 0);
			glVertexAttribDivisor(4, 0);
			glVertexAttribDivisor(5, 0);
			glVertexAttribDivisor(6, 0);

			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4);
			glDisableVertexAttribArray(5);
			glDisableVertexAttribArray(6);
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0); // Prevent buffer state leakage

			// Clean up texture state to prevent leakage between meshes/models
			for (unsigned int i = 0; i < mesh.textures.size(); i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			glActiveTexture(GL_TEXTURE0);
		}
	}

	void InstanceManager::RenderDotGroup(Shader& shader, InstanceGroup& group) {
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
			glBindBuffer(GL_ARRAY_BUFFER, 0); // Clean up before early return
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

		// Validate EBO binding
		GLint current_ebo = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &current_ebo);
		if (current_ebo == 0 && Shape::sphere_ebo_ != 0) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Shape::sphere_ebo_);
		}

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
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Prevent buffer state leakage
		shader.setBool("useInstanceColor", false);
	}

	void InstanceManager::RenderLineGroup(Shader& shader, InstanceGroup& group) {
		auto first_line = std::dynamic_pointer_cast<Line>(group.shapes[0]);
		if (!first_line || !IsValidVAO(Line::line_vao_))
			return;

		shader.setBool("isLine", true);
		shader.setInt("lineStyle", static_cast<int>(first_line->GetStyle()));
		shader.setBool("useInstanceColor", true);

		std::vector<glm::mat4> model_matrices;
		std::vector<glm::vec4> colors;
		model_matrices.reserve(group.shapes.size());
		colors.reserve(group.shapes.size());

		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());
			colors.emplace_back(shape->GetR(), shape->GetG(), shape->GetB(), shape->GetA());
		}

		if (group.instance_matrix_vbo_ == 0)
			glGenBuffers(1, &group.instance_matrix_vbo_);
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

		if (group.instance_color_vbo_ == 0)
			glGenBuffers(1, &group.instance_color_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		if (colors.size() > group.color_capacity_) {
			glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), &colors[0], GL_DYNAMIC_DRAW);
			group.color_capacity_ = colors.size();
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, colors.size() * sizeof(glm::vec4), &colors[0]);
		}

		glBindVertexArray(Line::line_vao_);
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glVertexAttribDivisor(7, 1);

		glDrawArraysInstanced(GL_TRIANGLES, 0, Line::line_vertex_count_, group.shapes.size());

		for (int i = 0; i < 5; i++) {
			glVertexAttribDivisor(3 + i, 0);
			glDisableVertexAttribArray(3 + i);
		}
		glBindVertexArray(0);
		shader.setBool("isLine", false);
		shader.setBool("useInstanceColor", false);
	}

	void InstanceManager::RenderCheckpointRingGroup(Shader& /*shader*/, InstanceGroup& group) {
		auto first_ring = std::dynamic_pointer_cast<CheckpointRingShape>(group.shapes[0]);
		if (!first_ring || !IsValidVAO(CheckpointRingShape::quad_vao_))
			return;

		auto cp_shader = CheckpointRingShape::GetShader();
		if (!cp_shader)
			return;

		cp_shader->use();
		cp_shader->setBool("is_instanced", true);

		// Set uniforms based on first ring (they should all be same style/radius due to InstanceKey)
		cp_shader->setInt("style", static_cast<int>(first_ring->GetStyle()));
		cp_shader->setFloat("radius", first_ring->GetRadius());

		std::vector<glm::mat4> model_matrices;
		std::vector<glm::vec4> colors;
		model_matrices.reserve(group.shapes.size());
		colors.reserve(group.shapes.size());

		for (const auto& shape : group.shapes) {
			model_matrices.push_back(shape->GetModelMatrix());

			auto ring = std::dynamic_pointer_cast<CheckpointRingShape>(shape);
			glm::vec3 color(shape->GetR(), shape->GetG(), shape->GetB());
			switch (ring->GetStyle()) {
			case CheckpointStyle::GOLD:
				color = Constants::Class::Checkpoint::Colors::Gold();
				break;
			case CheckpointStyle::SILVER:
				color = Constants::Class::Checkpoint::Colors::Silver();
				break;
			case CheckpointStyle::BLACK:
				color = Constants::Class::Checkpoint::Colors::Black();
				break;
			case CheckpointStyle::BLUE:
				color = Constants::Class::Checkpoint::Colors::Blue();
				break;
			case CheckpointStyle::NEON_GREEN:
				color = Constants::Class::Checkpoint::Colors::NeonGreen();
				break;
			}
			colors.emplace_back(color.r, color.g, color.b, shape->GetA());
		}

		if (group.instance_matrix_vbo_ == 0)
			glGenBuffers(1, &group.instance_matrix_vbo_);
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

		if (group.instance_color_vbo_ == 0)
			glGenBuffers(1, &group.instance_color_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		if (colors.size() > group.color_capacity_) {
			glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), &colors[0], GL_DYNAMIC_DRAW);
			group.color_capacity_ = colors.size();
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, colors.size() * sizeof(glm::vec4), &colors[0]);
		}

		glBindVertexArray(CheckpointRingShape::quad_vao_);
		glBindBuffer(GL_ARRAY_BUFFER, group.instance_matrix_vbo_);
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		glBindBuffer(GL_ARRAY_BUFFER, group.instance_color_vbo_);
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glVertexAttribDivisor(7, 1);

		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, group.shapes.size());

		for (int i = 0; i < 4; i++) {
			glVertexAttribDivisor(3 + i, 0);
			glDisableVertexAttribArray(3 + i);
		}
		glVertexAttribDivisor(7, 0);
		glDisableVertexAttribArray(7);
		glBindVertexArray(0);

		cp_shader->setBool("is_instanced", false);

		// Switch back to main shader
		if (Shape::shader)
			Shape::shader->use();
	}

} // namespace Boidsish
