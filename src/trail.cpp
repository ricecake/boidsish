#include "trail.h"

#include <algorithm>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	Trail::Trail(int max_length): max_length(max_length) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}

	Trail::~Trail() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}

	void Trail::AddPosition(float x, float y, float z) {
		positions.push_back(glm::vec3(x, y, z));
		if (positions.size() > static_cast<size_t>(max_length)) {
			positions.pop_front();
		}
	}

	void Trail::Render(Shader& shader, float r, float g, float b, const glm::vec3& camera_pos) const {
		if (positions.size() < 2) {
			return;
		}

		struct TrailVertex {
			glm::vec3 pos;
			float     progress;
		};

		std::vector<TrailVertex> vertices;
		for (size_t i = 0; i < positions.size() - 1; ++i) {
			glm::vec3 p0 = positions[i];
			glm::vec3 p1 = positions[i+1];

			glm::vec3 dir = glm::normalize(p1 - p0);
			glm::vec3 to_camera = glm::normalize(camera_pos - p0);
			glm::vec3 offset = glm::normalize(glm::cross(dir, to_camera)) * 0.05f;

			vertices.push_back({p0 + offset, (float)i / (float)positions.size()});
			vertices.push_back({p0 - offset, (float)i / (float)positions.size()});
		}

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TrailVertex), &vertices[0], GL_DYNAMIC_DRAW);

		// Position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
		glEnableVertexAttribArray(0);

		// Progress
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, progress));
		glEnableVertexAttribArray(1);

		shader.use();
		shader.setVec3("color", r, g, b);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
		glBindVertexArray(0);
	}

} // namespace Boidsish
