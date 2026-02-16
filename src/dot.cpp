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
		Shape(id, x, y, z, r, g, b, a, trail_length), size_(size) {
		// SetInstanced(true);
	}

	void Dot::render() const {
		render(*shader, GetModelMatrix());
	}

	void Dot::render(Shader& shader, const glm::mat4& model_matrix) const {
		// Ensure sphere VAO is initialized
		if (sphere_vao_ == 0)
			return;

		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());
		shader.setBool("use_texture", false);

		// Set PBR material properties
		shader.setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader.setFloat("roughness", GetRoughness());
			shader.setFloat("metallic", GetMetallic());
			shader.setFloat("ao", GetAO());
		}

		glBindVertexArray(sphere_vao_);
		glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
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

	bool Dot::Intersects(const Ray& ray, float& t) const {
		glm::vec3 center(GetX(), GetY(), GetZ());
		float     radius = size_ * 0.01f * GetScale().x; // Assuming uniform scale for dot
		glm::vec3 L = center - ray.origin;
		float     tca = glm::dot(L, ray.direction);
		if (tca < 0)
			return false;
		float d2 = glm::dot(L, L) - tca * tca;
		float r2 = radius * radius;
		if (d2 > r2)
			return false;
		float thc = sqrt(r2 - d2);
		t = tca - thc;
		return true;
	}

	AABB Dot::GetAABB() const {
		glm::vec3 center(GetX(), GetY(), GetZ());
		float     radius = size_ * 0.01f * GetScale().x;
		return AABB(center - glm::vec3(radius), center + glm::vec3(radius));
	}

} // namespace Boidsish
