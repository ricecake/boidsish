#include "instance_manager.h"

#include <set>

#include "dot.h"
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

	void InstanceManager::Render(Shader& shader, bool ignore_occlusion) {
		shader.use();
		shader.setBool("is_instanced", true);

		for (auto& [key, group] : m_instance_groups) {
			if (group.shapes.empty()) {
				continue;
			}

			// Filter out occluded shapes
			std::vector<std::shared_ptr<Shape>> visible_shapes;
			for (auto& shape : group.shapes) {
				// Update occlusion query result
				if (shape->IsQueryIssued()) {
					GLuint available = 0;
					glGetQueryObjectuiv(shape->GetOcclusionQuery(), GL_QUERY_RESULT_AVAILABLE, &available);
					if (available) {
						GLuint samples = 0;
						glGetQueryObjectuiv(shape->GetOcclusionQuery(), GL_QUERY_RESULT, &samples);
						shape->SetOccluded(samples == 0);
						shape->SetQueryIssued(false);
					}
					// If query results are not yet available, we retain the state from the previous frame
					// to prevent 1-frame flickering.
				}

				if (ignore_occlusion || shape->IsColossal() || !shape->IsOccluded()) {
					visible_shapes.push_back(shape);
				}
			}

			if (visible_shapes.empty()) {
				group.shapes.clear();
				continue;
			}

			// Temporarily replace group shapes with visible ones for rendering
			std::vector<std::shared_ptr<Shape>> original_shapes = std::move(group.shapes);
			group.shapes = std::move(visible_shapes);

			// Determine type from the key prefix
			if (key.starts_with("Model:")) {
				RenderModelGroup(shader, group);
			} else if (key == "Dot") {
				RenderDotGroup(shader, group);
			}
			// Other shape types can be added here

			// Restore original shapes so they can be queried again next frame
			group.shapes = std::move(original_shapes);
			group.shapes.clear();
		}

		// Reset all shader uniforms to safe defaults after all groups rendered
		shader.setBool("is_instanced", false);
		shader.setBool("useInstanceColor", false);
		shader.setBool("isColossal", false);
		shader.setBool("usePBR", false);
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
			// (requires update to mesh.render_instanced to support this flag)
			mesh.render_instanced(group.shapes.size(), false);

			// Note: render_instanced(..., false) now DOES NOT unbind the VAO
			// allowing us to clean up efficiently.

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

	void InstanceManager::RenderOcclusionQueries(Shader& shader) {
		shader.use();

		static GLuint bbox_vao = 0, bbox_vbo = 0, bbox_ebo = 0;
		if (bbox_vao == 0) {
			glGenVertexArrays(1, &bbox_vao);
			glGenBuffers(1, &bbox_vbo);
			glGenBuffers(1, &bbox_ebo);
			float vertices[] = {
				0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
			};
			unsigned int indices[] = {
				0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 1, 5, 5, 4, 0,
				2, 3, 7, 7, 6, 2, 1, 2, 6, 6, 5, 1, 0, 3, 7, 7, 4, 0,
			};
			glBindVertexArray(bbox_vao);
			glBindBuffer(GL_ARRAY_BUFFER, bbox_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbox_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}

		// Switch to query mode
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_LEQUAL);
		glBindVertexArray(bbox_vao);

		for (auto& [key, group] : m_instance_groups) {
			for (auto& shape : group.shapes) {
				if (shape->IsHidden())
					continue;

				auto model = std::dynamic_pointer_cast<Model>(shape);
				if (!model)
					continue;

				if (shape->GetOcclusionQuery() == 0) {
					GLuint query;
					glGenQueries(1, &query);
					shape->SetOcclusionQuery(query);
				}

				if (shape->IsQueryIssued())
					continue;

				// Use AABB for the query volume
				glm::vec3 min_b = model->GetMinBound();
				glm::vec3 max_b = model->GetMaxBound();
				glm::vec3 size  = max_b - min_b;

				// Inflate slightly
				float     padding = 0.1f;
				glm::mat4 model_matrix = shape->GetModelMatrix();
				model_matrix           = glm::translate(model_matrix, min_b - glm::vec3(padding * 0.5f));
				model_matrix           = glm::scale(model_matrix, size + glm::vec3(padding));

				shader.setMat4("model", model_matrix);

				glBeginQuery(GL_ANY_SAMPLES_PASSED, shape->GetOcclusionQuery());
				glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
				glEndQuery(GL_ANY_SAMPLES_PASSED);
				shape->SetQueryIssued(true);
			}
		}

		glBindVertexArray(0);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
	}

	void InstanceManager::ClearInstances() {
		for (auto& [key, group] : m_instance_groups) {
			group.shapes.clear();
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

} // namespace Boidsish
