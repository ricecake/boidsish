#include "debug_cone.h"

#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	DebugCone::DebugCone(int id, float x, float y, float z, float r, float g, float b, float a):
		Shape(id, x, y, z, r, g, b, a) {
		SetInstanced(true);
	}

	void DebugCone::render() const {
		if (cone_vao_ == 0)
			return;

		shader->use();
		shader->setMat4("model", GetModelMatrix());
		shader->setVec3("objectColor", GetR(), GetG(), GetB());
		shader->setFloat("objectAlpha", GetA());

		glBindVertexArray(cone_vao_);
		glDrawElements(GL_TRIANGLES, cone_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void DebugCone::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (cone_vao_ == 0)
			return;

		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());

		glBindVertexArray(cone_vao_);
		glDrawElements(GL_TRIANGLES, cone_vertex_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glm::mat4 DebugCone::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale());
		return model;
	}

} // namespace Boidsish
