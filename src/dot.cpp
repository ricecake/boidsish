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
		RenderSphere(glm::vec3(GetX(), GetY(), GetZ()), rotation_, glm::vec3(GetR(), GetG(), GetB()), size_ * 0.01f);
	}

} // namespace Boidsish
