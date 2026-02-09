#include "instance_manager.h"

#include "dot.h"
#include "model.h"
#include <GL/glew.h>

namespace Boidsish {

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
			}
			// Other shape types can be added here
			group.shapes.clear();
		}

		shader.setBool("is_instanced", false);
	}

	void InstanceManager::RenderModelGroup(Shader& shader, InstanceGroup& group) {
		// Build instance matrices, filtering by occlusion
		std::vector<glm::mat4> model_matrices;
		model_matrices.reserve(group.shapes.size());
		for (const auto& shape : group.shapes) {
			auto q_it = m_queries.find(shape->GetId());
			if (q_it != m_queries.end() && q_it->second.is_occluded) {
				continue;
			}
			model_matrices.push_back(shape->GetModelMatrix());
		}

		if (model_matrices.empty())
			return;

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

	void InstanceManager::PerformOcclusionQueries(
		const glm::mat4&                    view,
		const glm::mat4&                    projection,
		Shader&                             shader,
		const std::vector<std::shared_ptr<Shape>>& shapes
	) {
		m_frame_count++;

		// 1. Gather results from previous frame
		for (auto& [id, query] : m_queries) {
			if (!query.query_issued)
				continue;

			GLuint available = 0;
			glGetQueryObjectuiv(query.query_id, GL_QUERY_RESULT_AVAILABLE, &available);
			if (available) {
				GLuint samples = 0;
				glGetQueryObjectuiv(query.query_id, GL_QUERY_RESULT, &samples);
				query.is_occluded = (samples == 0);
				query.query_issued = false;
			}
		}

		// 2. Issue new queries for all model instances
		static GLuint bbox_vao = 0, bbox_vbo = 0, bbox_ebo = 0;
		if (bbox_vao == 0) {
			glGenVertexArrays(1, &bbox_vao);
			glGenBuffers(1, &bbox_vbo);
			glGenBuffers(1, &bbox_ebo);
			float vertices[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1};
			unsigned int indices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 1, 5, 5, 4, 0,
			                          2, 3, 7, 7, 6, 2, 1, 2, 6, 6, 5, 1, 0, 3, 7, 7, 4, 0};
			glBindVertexArray(bbox_vao);
			glBindBuffer(GL_ARRAY_BUFFER, bbox_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbox_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}

		glBindVertexArray(bbox_vao);
		GLint modelLoc = glGetUniformLocation(shader.ID, "model");

		for (const auto& shape : shapes) {
			if (shape->IsHidden()) {
				auto q_it = m_queries.find(shape->GetId());
				if (q_it != m_queries.end()) {
					q_it->second.is_occluded = false;
				}
				continue;
			}

			std::string key = shape->GetInstanceKey();
			if (!key.starts_with("Model:"))
				continue;

			auto model = std::dynamic_pointer_cast<Model>(shape);
			if (!model)
				continue;

			glm::vec3 min_aabb, max_aabb;
			model->GetAABB(min_aabb, max_aabb);
			glm::vec3 size = max_aabb - min_aabb;

			auto& q_info = m_queries[shape->GetId()];
			if (q_info.query_id == 0) {
				glGenQueries(1, &q_info.query_id);
			}
			q_info.last_frame_used = m_frame_count;

			glm::mat4 model_mat = shape->GetModelMatrix();
			// Adjust model matrix to match the AABB
			glm::mat4 bbox_mat = glm::translate(model_mat, min_aabb);
			bbox_mat = glm::scale(bbox_mat, size);

			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &bbox_mat[0][0]);

			glBeginQuery(GL_ANY_SAMPLES_PASSED, q_info.query_id);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			glEndQuery(GL_ANY_SAMPLES_PASSED);
			q_info.query_issued = true;
		}
		glBindVertexArray(0);

		// 3. Cleanup unused queries
		auto it = m_queries.begin();
		while (it != m_queries.end()) {
			if (it->second.last_frame_used < m_frame_count - 10) {
				glDeleteQueries(1, &it->second.query_id);
				it = m_queries.erase(it);
			} else {
				++it;
			}
		}
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
