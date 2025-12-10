#include "trail.h"

#include <algorithm>
#include <vector>

namespace Boidsish {

	Trail::Trail(int max_length): max_length(max_length) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}

	Trail::~Trail() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});
		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
		}

		if (points.size() < 2) {
			return; // Not enough points for a line
		}

		struct TrailVertex {
			glm::vec3 pos;
			glm::vec3 color;
			float     progress;
		};

		std::vector<TrailVertex> vertices;
		for (size_t i = 0; i < points.size(); ++i) {
			float progress = (float)i / (float)(points.size() - 1);
			vertices.push_back({points[i].first, points[i].second, progress});
		}

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TrailVertex), &vertices[0], GL_DYNAMIC_DRAW);

		// Position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
		glEnableVertexAttribArray(0);

		// Color
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));
		glEnableVertexAttribArray(1);

		// Progress
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, progress));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	void Trail::Render(
			Shader& shader, const glm::vec3& view_pos, const glm::vec3& light_pos) const {
		if (points.size() < 4) {
			return;
		}

		shader.use();
		shader.setFloat("thickness", 0.1f);
		shader.setVec3("viewPos", view_pos);
		shader.setVec3("lightPos", light_pos);
		shader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);

		glBindVertexArray(vao);
		glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, points.size());
		glBindVertexArray(0);
	}

} // namespace Boidsish
