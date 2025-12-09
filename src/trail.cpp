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

	void Trail::AddPosition(float x, float y, float z) {
		positions.push_back(glm::vec3(x, y, z));
		if (positions.size() > static_cast<size_t>(max_length)) {
			positions.pop_front();
		}

		if (positions.size() < 2) {
			return; // Not enough points for a line
		}

		struct TrailVertex {
			glm::vec3 pos;
			float     progress;
		};

		std::vector<TrailVertex> vertices;
		// Create explicit line segments: (v0, v1), (v1, v2), (v2, v3)...
		for (size_t i = 0; i < positions.size() - 1; ++i) {
			float progress_start = (float)i / (float)(positions.size() - 1);
			float progress_end = (float)(i + 1) / (float)(positions.size() - 1);
			vertices.push_back({positions[i], progress_start});
			vertices.push_back({positions[i + 1], progress_end});
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

		glBindVertexArray(0);
	}

	void Trail::Render(Shader& shader, float r, float g, float b) const {
		if (positions.size() < 2) {
			return;
		}

		shader.use();
		shader.setVec3("color", r, g, b);
		shader.setFloat("thickness", 0.05f);

		glBindVertexArray(vao);
		// Draw N-1 line segments, each with 2 vertices
		glDrawArrays(GL_LINES, 0, (positions.size() - 1) * 2);
		glBindVertexArray(0);
	}

} // namespace Boidsish
