#pragma once

#include <deque>
#include <tuple>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class Trail {
	public:
		Trail(int max_length = 250);
		~Trail();

		void AddPoint(glm::vec3 position, glm::vec3 color);
		void Render(Shader& shader) const;

	private:
		std::deque<std::pair<glm::vec3, glm::vec3>> points;
		int                   max_length;
		GLuint                vao;
		GLuint                vbo;
		int                   vertex_count;
	};

} // namespace Boidsish
