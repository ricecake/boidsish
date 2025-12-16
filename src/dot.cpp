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
		RenderSphere(glm::vec3(GetX(), GetY(), GetZ()), glm::vec3(GetR(), GetG(), GetB()), size_ * 0.01f);
	}

	void Dot::render(Shader& shader) const {
		shader.use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = glm::scale(model, glm::vec3(size_ * 0.01f));
		shader.setMat4("model", model);
		glBindVertexArray(sphere_vao_);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, sphere_vertex_count_);
		glBindVertexArray(0);
	}

} // namespace Boidsish
