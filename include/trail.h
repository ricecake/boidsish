#pragma once

#include <deque>
#include <tuple>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "shader.h"

namespace Boidsish {

	class Trail {
	public:
		Trail(int max_length = 250);
		~Trail();

		void AddPosition(float x, float y, float z);
		void Render(Shader& shader, float r, float g, float b, const glm::vec3& camera_pos) const;

	private:
		std::deque<glm::vec3> positions;
		int max_length;
		GLuint vao;
		GLuint vbo;
	};

} // namespace Boidsish
