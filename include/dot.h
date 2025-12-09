#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"
#include <GL/glew.h>

namespace Boidsish {

	// Class representing a single dot/particle, inheriting from Shape
	class Dot: public Shape {
	public:
		float size; // Size of the dot

		Dot(int   id = 0,
		    float x = 0.0f,
		    float y = 0.0f,
		    float z = 0.0f,
		    float size = 1.0f,
		    float r = 1.0f,
		    float g = 1.0f,
		    float b = 1.0f,
		    float a = 1.0f,
		    int   trail_length = 10);

		void render(Shader& shader) const;
		void render() const override;

		// Static members for the shared sphere mesh
		static void InitSphereMesh();
		static void CleanupSphereMesh();
	private:
		static GLuint vao;
		static GLuint vbo;
		static GLuint ebo;
		static int vertex_count;
};
} // namespace Boidsish