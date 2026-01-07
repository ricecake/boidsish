#include "dot.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length):
		Shape(id, x, y, z, r, g, b, a, trail_length), size_(size) {}

	void Dot::render() const {
		shader->use();
		render(*shader, GetModelMatrix());
	}

	void Dot::render(Shader& shader, const glm::mat4& model_matrix) const {
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());

		glBindVertexArray(sphere_vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 3);
		glDrawElements(GL_PATCHES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glm::mat4 Dot::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		glm::vec3 combined_scale = GetScale() * size_ * 0.01f;
		model = glm::scale(model, combined_scale);
		return model;
	}

} // namespace Boidsish
