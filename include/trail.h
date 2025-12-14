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
		Trail(float thickness = 0.06f, int max_length = 250);
		~Trail();

		void AddPoint(glm::vec3 position, glm::vec3 color, float object_radius);
		void Render(Shader& shader) const;

	private:
		struct TrailVertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec3 color;
			float     progress;
		};

		// Catmull-Rom interpolation for smooth curves
		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) const;

		// (Re)calculates the entire trail's geometry from scratch
		void GenerateTrailGeometry();
		// Builds the renderable mesh from the cached geometry
		void BuildMeshFromGeometryCache();
		// Appends a new segment to the geometry cache
		void AppendToGeometryCache(
			const Vector3& p0,
			const Vector3& p1,
			const Vector3& p2,
			const Vector3& p3,
			const Vector3& c0,
			const Vector3& c1,
			const Vector3& c2,
			const Vector3& c3
		);
		// Removes the oldest segment from the geometry cache
		void PopFromGeometryCache();

		// Frame transport for maintaining smooth normal orientation
		Vector3
		TransportFrame(const Vector3& prev_normal, const Vector3& prev_tangent, const Vector3& curr_tangent) const;

		float                                       thickness_;
		int                                         max_length;
		std::deque<std::pair<glm::vec3, glm::vec3>> points;
		GLuint                                      vao;
		GLuint                                      vbo;

		// Mesh data
		mutable std::vector<TrailVertex> mesh_vertices;
		mutable int                      vertex_count;
		mutable bool                     mesh_dirty;

		// Cached geometry data for incremental updates
		mutable std::deque<Vector3> curve_positions;
		mutable std::deque<Vector3> curve_colors;
		mutable std::deque<Vector3> tangents;
		mutable std::deque<Vector3> normals;
		mutable std::deque<Vector3> binormals;

		// Configuration
		const int TRAIL_SEGMENTS = 8; // Circular segments around trail
		const int CURVE_SEGMENTS = 4; // Interpolation segments per point
	};

} // namespace Boidsish
