#include "arrow.h"

#include <cmath>
#include <vector>

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	Arrow::Arrow(
		int   id,
		float x,
		float y,
		float z,
		float cone_height,
		float cone_radius,
		float rod_radius,
		float r,
		float g,
		float b,
		float a
	):
		Shape(id, x, y, z, r, g, b, a), cone_height_(cone_height), cone_radius_(cone_radius), rod_radius_(rod_radius) {
		InitArrowMesh();
	}

	Arrow::~Arrow() {
		DestroyArrowMesh();
	}

	void Arrow::render() const {
		shader->use();
		render(*shader, GetModelMatrix());
	}

	void Arrow::render(Shader& shader, const glm::mat4& model_matrix) const {
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());

		// Render Rod
		glBindVertexArray(rod_vao_);
		glDrawArrays(GL_TRIANGLES, 0, rod_vertex_count_);

		// Render Cone
		glBindVertexArray(cone_vao_);
		glDrawArrays(GL_TRIANGLES, 0, cone_vertex_count_);

		glBindVertexArray(0);
	}

	glm::mat4 Arrow::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale());
		return model;
	}


	void Arrow::InitArrowMesh() {
		// Rod generation
		std::vector<float> rod_vertices;
		int                segments = 16;
		float              length = 1.0f - cone_height_;
		for (int i = 0; i < segments; ++i) {
			float angle1 = i * 2 * glm::pi<float>() / segments;
			float angle2 = (i + 1) * 2 * glm::pi<float>() / segments;

			glm::vec3 p1(rod_radius_ * cos(angle1), 0, rod_radius_ * sin(angle1));
			glm::vec3 p2(rod_radius_ * cos(angle2), 0, rod_radius_ * sin(angle2));
			glm::vec3 p3(rod_radius_ * cos(angle1), length, rod_radius_ * sin(angle1));
			glm::vec3 p4(rod_radius_ * cos(angle2), length, rod_radius_ * sin(angle2));

			// Side
			glm::vec3 n1 = glm::normalize(glm::vec3(p1.x, 0, p1.z));
			glm::vec3 n2 = glm::normalize(glm::vec3(p2.x, 0, p2.z));
			glm::vec3 n3 = glm::normalize(glm::vec3(p3.x, 0, p3.z));
			glm::vec3 n4 = glm::normalize(glm::vec3(p4.x, 0, p4.z));

			rod_vertices.insert(rod_vertices.end(), {p1.x, p1.y, p1.z, n1.x, n1.y, n1.z});
			rod_vertices.insert(rod_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			rod_vertices.insert(rod_vertices.end(), {p3.x, p3.y, p3.z, n3.x, n3.y, n3.z});

			rod_vertices.insert(rod_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			rod_vertices.insert(rod_vertices.end(), {p4.x, p4.y, p4.z, n4.x, n4.y, n4.z});
			rod_vertices.insert(rod_vertices.end(), {p3.x, p3.y, p3.z, n3.x, n3.y, n3.z});
		}
		rod_vertex_count_ = rod_vertices.size() / 6;

		glGenVertexArrays(1, &rod_vao_);
		glBindVertexArray(rod_vao_);
		glGenBuffers(1, &rod_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, rod_vbo_);
		glBufferData(GL_ARRAY_BUFFER, rod_vertices.size() * sizeof(float), rod_vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// Cone generation
		std::vector<float> cone_vertices;
		glm::vec3          tip(0, 1.0, 0);
		for (int i = 0; i < segments; ++i) {
			float angle1 = i * 2 * glm::pi<float>() / segments;
			float angle2 = (i + 1) * 2 * glm::pi<float>() / segments;

			glm::vec3 p1(cone_radius_ * cos(angle1), length, cone_radius_ * sin(angle1));
			glm::vec3 p2(cone_radius_ * cos(angle2), length, cone_radius_ * sin(angle2));

			// Side
			glm::vec3 n1 = glm::normalize(glm::vec3(p1.x, cone_radius_, p1.z));
			glm::vec3 n2 = glm::normalize(glm::vec3(p2.x, cone_radius_, p2.z));
			glm::vec3 tip_normal = glm::normalize(glm::cross(p2 - p1, tip - p1));

			cone_vertices.insert(cone_vertices.end(), {p1.x, p1.y, p1.z, n1.x, n1.y, n1.z});
			cone_vertices.insert(cone_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			cone_vertices.insert(cone_vertices.end(), {tip.x, tip.y, tip.z, tip_normal.x, tip_normal.y, tip_normal.z});

			// Base
			glm::vec3 normal = glm::vec3(0, -1, 0);
			cone_vertices.insert(cone_vertices.end(), {p1.x, p1.y, p1.z, normal.x, normal.y, normal.z});
			cone_vertices.insert(cone_vertices.end(), {p2.x, p2.y, p2.z, normal.x, normal.y, normal.z});
			cone_vertices.insert(cone_vertices.end(), {0, length, 0, normal.x, normal.y, normal.z});
		}
		cone_vertex_count_ = cone_vertices.size() / 6;

		glGenVertexArrays(1, &cone_vao_);
		glBindVertexArray(cone_vao_);
		glGenBuffers(1, &cone_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, cone_vbo_);
		glBufferData(GL_ARRAY_BUFFER, cone_vertices.size() * sizeof(float), cone_vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	void Arrow::SetDirection(const glm::vec3& direction) {
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 norm_direction = glm::normalize(direction);

		float dot = glm::dot(up, norm_direction);
		if (abs(dot + 1.0f) < 0.000001f) {
			// vector a and b are parallel but opposite direction
			SetRotation(glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
			return;
		}
		if (abs(dot - 1.0f) < 0.000001f) {
			// vector a and b are parallel in same direction
			SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		glm::vec3 axis = glm::cross(up, norm_direction);
		float     angle = acos(dot);
		SetRotation(glm::angleAxis(angle, axis));
	}

	void Arrow::DestroyArrowMesh() {
		if (rod_vao_ != 0) {
			glDeleteVertexArrays(1, &rod_vao_);
			glDeleteBuffers(1, &rod_vbo_);
			rod_vao_ = 0;
		}
		if (cone_vao_ != 0) {
			glDeleteVertexArrays(1, &cone_vao_);
			glDeleteBuffers(1, &cone_vbo_);
			cone_vao_ = 0;
		}
	}
} // namespace Boidsish
