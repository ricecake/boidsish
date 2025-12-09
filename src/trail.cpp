#include "trail.h"

#include <algorithm>

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

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), &positions[0], GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glBindVertexArray(0);
	}

	void Trail::Render(Shader& shader, float r, float g, float b) const {
		if (positions.size() < 2) {
			return;
		}

		shader.use();
		shader.setVec3("color", r, g, b);

		glBindVertexArray(vao);
		glDrawArrays(GL_LINE_STRIP, 0, positions.size());
		glBindVertexArray(0);
	}

} // namespace Boidsish
