#pragma once

#include <deque>
#include <tuple>
#include <vector>

#include "shader.h"
#include "vector.h"
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
		struct TrailVertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec3 color;
		};

		// Catmull-Rom interpolation for smooth curves
		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) const;

		// Generate continuous mesh with frame transport
		void GenerateMesh();

		// Frame transport for maintaining smooth normal orientation
		Vector3
		TransportFrame(const Vector3& prev_normal, const Vector3& prev_tangent, const Vector3& curr_tangent) const;

		std::deque<std::pair<glm::vec3, glm::vec3>> points;
		int                                         max_length;
		GLuint                                      vao;
		GLuint                                      vbo;

		// Mesh data
		mutable std::vector<TrailVertex> mesh_vertices;
		mutable int                      vertex_count;
		mutable bool                     mesh_dirty;

		// Configuration
		const int   TRAIL_SEGMENTS = 8;     // Circular segments around trail
		const int   CURVE_SEGMENTS = 4;     // Interpolation segments per point
		const float BASE_THICKNESS = 0.06f; // Maximum thickness at trail start
	};

} // namespace Boidsish
