#include "trail.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "shape.h"

namespace Boidsish {

	Trail::Trail(int max_length): max_length(max_length), vertex_count(0), mesh_dirty(false) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}

	Trail::~Trail() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}

	Vector3
	Trail::CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) const {
		return 0.5f *
			((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
		     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
	}

	Vector3
	Trail::TransportFrame(const Vector3& prev_normal, const Vector3& prev_tangent, const Vector3& curr_tangent) const {
		// Frame transport: rotate the previous normal to maintain smooth orientation
		Vector3 axis = prev_tangent.Cross(curr_tangent);
		float   axis_length = axis.Magnitude();

		if (axis_length < 1e-6f) {
			// Tangents are parallel, no rotation needed
			return prev_normal;
		}

		axis = axis / axis_length;
		float angle = acos(std::clamp(prev_tangent.Dot(curr_tangent), -1.0f, 1.0f));

		// Rodrigues' rotation formula
		Vector3 rotated = prev_normal * cos(angle) + axis.Cross(prev_normal) * sin(angle) +
			axis * (axis.Dot(prev_normal)) * (1 - cos(angle));

		return rotated.Normalized();
	}

	void Trail::GenerateTrailGeometry() {
		curve_positions.clear();
		curve_colors.clear();
		tangents.clear();
		normals.clear();
		binormals.clear();

		if (points.size() < 4) {
			return;
		}

		std::vector<Vector3> positions;
		std::vector<Vector3> colors;
		for (const auto& point : points) {
			positions.emplace_back(point.first.x, point.first.y, point.first.z);
			colors.emplace_back(point.second.x, point.second.y, point.second.z);
		}

		for (int i = 0; i < (int)positions.size() - 3; ++i) {
			AppendToGeometryCache(
				positions[i],
				positions[i + 1],
				positions[i + 2],
				positions[i + 3],
				colors[i],
				colors[i + 1],
				colors[i + 2],
				colors[i + 3]
			);
		}
	}

	void Trail::BuildMeshFromGeometryCache() {
		mesh_vertices.clear();

		if (curve_positions.size() < 2) {
			vertex_count = 0;
			return;
		}

		std::vector<float> curve_progress;
		for (int i = 0; i < curve_positions.size(); ++i) {
			curve_progress.push_back((float)i / (curve_positions.size() - 1));
		}

		if (curve_positions.size() < 2) {
			vertex_count = 0;
			return;
		}

		tangents.clear();
		normals.clear();
		binormals.clear();

		for (int i = 0; i < (int)curve_positions.size(); ++i) {
			Vector3 tangent;
			if (i == 0) {
				tangent = (curve_positions[i + 1] - curve_positions[i]).Normalized();
			} else if (i == (int)curve_positions.size() - 1) {
				tangent = (curve_positions[i] - curve_positions[i - 1]).Normalized();
			} else {
				tangent = (curve_positions[i + 1] - curve_positions[i - 1]).Normalized();
			}
			tangents.push_back(tangent);
		}

		// Initialize first frame
		Vector3 initial_normal;
		if (abs(tangents[0].y) < 0.999f) {
			initial_normal = tangents[0].Cross(Vector3(0, 1, 0)).Normalized();
		} else {
			initial_normal = tangents[0].Cross(Vector3(1, 0, 0)).Normalized();
		}

		normals.push_back(initial_normal);
		binormals.push_back(tangents[0].Cross(initial_normal).Normalized());

		// Transport frame along the curve
		for (int i = 1; i < (int)curve_positions.size(); ++i) {
			Vector3 transported_normal = TransportFrame(normals[i - 1], tangents[i - 1], tangents[i]);
			normals.push_back(transported_normal);
			binormals.push_back(tangents[i].Cross(transported_normal).Normalized());
		}

		std::vector<std::vector<glm::vec3>> ring_positions;
		std::vector<std::vector<glm::vec3>> ring_normals;

		// Precompute all rings to ensure seamless connections
		for (size_t i = 0; i < curve_positions.size(); ++i) {
			std::vector<glm::vec3> current_ring_pos;
			std::vector<glm::vec3> current_ring_norm;
			float                  thickness = BASE_THICKNESS;

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				float   angle = 2.0f * std::numbers::pi * j / TRAIL_SEGMENTS;
				Vector3 offset = (normals[i] * cos(angle) + binormals[i] * sin(angle)) * thickness;
				Vector3 pos = curve_positions[i] + offset;
				Vector3 norm = offset.Normalized();
				current_ring_pos.emplace_back(pos.x, pos.y, pos.z);
				current_ring_norm.emplace_back(norm.x, norm.y, norm.z);
			}
			ring_positions.push_back(current_ring_pos);
			ring_normals.push_back(current_ring_norm);
		}

		// Generate cylindrical mesh from precomputed rings
		for (int i = 0; i < (int)curve_positions.size() - 1; ++i) {
			const auto& ring1_positions = ring_positions[i];
			const auto& ring1_normals = ring_normals[i];
			const auto& ring2_positions = ring_positions[i + 1];
			const auto& ring2_normals = ring_normals[i + 1];

			for (int j = 0; j < TRAIL_SEGMENTS; ++j) {
				// First triangle (CCW)
				mesh_vertices.push_back(
					{ring1_positions[j],
				     ring1_normals[j],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				     curve_progress[i]}
				);
				mesh_vertices.push_back(
					{ring1_positions[j + 1],
				     ring1_normals[j + 1],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				     curve_progress[i]}
				);
				mesh_vertices.push_back(
					{ring2_positions[j],
				     ring2_normals[j],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z),
				     curve_progress[i + 1]}
				);

				// Second triangle (CCW)
				mesh_vertices.push_back(
					{ring1_positions[j + 1],
				     ring1_normals[j + 1],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				     curve_progress[i]}
				);
				mesh_vertices.push_back(
					{ring2_positions[j + 1],
				     ring2_normals[j + 1],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z),
				     curve_progress[i + 1]}
				);
				mesh_vertices.push_back(
					{ring2_positions[j],
				     ring2_normals[j],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z),
				     curve_progress[i + 1]}
				);
			}
		}

		vertex_count = mesh_vertices.size();
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});

		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
			PopFromGeometryCache();
		}

		if (points.size() >= 4) {
			int                  size = points.size();
			std::vector<Vector3> p(4);
			std::vector<Vector3> c(4);

			for (int i = 0; i < 4; ++i) {
				p[i] = Vector3(
					points[size - 4 + i].first.x,
					points[size - 4 + i].first.y,
					points[size - 4 + i].first.z
				);
				c[i] = Vector3(
					points[size - 4 + i].second.x,
					points[size - 4 + i].second.y,
					points[size - 4 + i].second.z
				);
			}

			AppendToGeometryCache(p[0], p[1], p[2], p[3], c[0], c[1], c[2], c[3]);
		}

		mesh_dirty = true;
	}

	void Trail::Render(Shader& shader) const {
		if (points.size() < 4) {
			return;
		}

		if (mesh_dirty) {
			if (curve_positions.empty() && points.size() >= 4) {
				const_cast<Trail*>(this)->GenerateTrailGeometry();
			}
			const_cast<Trail*>(this)->BuildMeshFromGeometryCache();
			const_cast<Trail*>(this)->mesh_dirty = false;

			// Upload mesh to GPU
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(
				GL_ARRAY_BUFFER,
				mesh_vertices.size() * sizeof(TrailVertex),
				mesh_vertices.data(),
				GL_DYNAMIC_DRAW
			);

			// Position
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
			glEnableVertexAttribArray(0);

			// Normal
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, normal));
			glEnableVertexAttribArray(1);

			// Color
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));
			glEnableVertexAttribArray(2);

			// Progress
			glVertexAttribPointer(
				3,
				1,
				GL_FLOAT,
				GL_FALSE,
				sizeof(TrailVertex),
				(void*)offsetof(TrailVertex, progress)
			);
			glEnableVertexAttribArray(3);

			glBindVertexArray(0);
		}

		shader.use();
		shader.setFloat("base_thickness", BASE_THICKNESS);
		shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count);
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);

		// Render a cap at the end of the trail
		if (!points.empty()) {
			const auto& end_point = points.front();
			float       end_thickness = BASE_THICKNESS * 0.1f; // Make cap size relative to trail
			Shape::RenderSphere(end_point.first, end_point.second, end_thickness);
		}
	}

	void Trail::AppendToGeometryCache(
		const Vector3& p0,
		const Vector3& p1,
		const Vector3& p2,
		const Vector3& p3,
		const Vector3& c0,
		const Vector3& c1,
		const Vector3& c2,
		const Vector3& c3
	) {
		for (int j = 0; j < CURVE_SEGMENTS; ++j) {
			float   t = (float)j / CURVE_SEGMENTS;
			Vector3 pos = CatmullRom(t, p0, p1, p2, p3);
			Vector3 color = CatmullRom(t, c0, c1, c2, c3);

			curve_positions.push_back(pos);
			curve_colors.push_back(color);
		}
	}

	void Trail::PopFromGeometryCache() {
		for (int i = 0; i < CURVE_SEGMENTS; ++i) {
			curve_positions.pop_front();
			curve_colors.pop_front();
		}
	}

} // namespace Boidsish
