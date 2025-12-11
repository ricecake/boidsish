#include "trail.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

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

	void Trail::GenerateMesh() {
		mesh_vertices.clear();

		if (points.size() < 4) {
			vertex_count = 0;
			return;
		}

		std::vector<Vector3> positions;
		std::vector<Vector3> colors;

		for (const auto& point : points) {
			positions.emplace_back(point.first.x, point.first.y, point.first.z);
			colors.emplace_back(point.second.x, point.second.y, point.second.z);
		}

		// Generate smooth curve using Catmull-Rom splines
		std::vector<Vector3> curve_positions;
		std::vector<Vector3> curve_colors;
		std::vector<float>   curve_progress;

		int total_segments = (positions.size() - 3) * CURVE_SEGMENTS;
		for (int i = 0; i < (int)positions.size() - 3; ++i) {
			for (int j = 0; j < CURVE_SEGMENTS; ++j) {
				float   t = (float)j / CURVE_SEGMENTS;
				Vector3 pos = CatmullRom(t, positions[i], positions[i + 1], positions[i + 2], positions[i + 3]);
				Vector3 color = CatmullRom(t, colors[i], colors[i + 1], colors[i + 2], colors[i + 3]);

				float progress = (float)(i * CURVE_SEGMENTS + j) / total_segments;

				curve_positions.push_back(pos);
				curve_colors.push_back(color);
				curve_progress.push_back(progress);
			}
		}

		// Add the final point
		curve_positions.push_back(positions[positions.size() - 2]);
		curve_colors.push_back(colors[colors.size() - 2]);
		curve_progress.push_back(1.0f);

		if (curve_positions.size() < 2) {
			vertex_count = 0;
			return;
		}

		// Generate tangents and frame transport
		std::vector<Vector3> tangents;
		std::vector<Vector3> normals;
		std::vector<Vector3> binormals;

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

		// Generate cylindrical mesh around the curve
		for (int i = 0; i < (int)curve_positions.size() - 1; ++i) {
			// Calculate thickness based on progress (smaller towards end)
			// Progress 0 = oldest (tail), Progress 1 = newest (head)
			// We want thick at head (newest) and thin at tail (oldest)
			float thickness1 = BASE_THICKNESS * (0.2f + curve_progress[i] * 0.8f); // 20% to 100%
			float thickness2 = BASE_THICKNESS * (0.2f + curve_progress[i + 1] * 0.8f);

			// Create rings of vertices
			std::vector<glm::vec3> ring1_positions, ring1_normals;
			std::vector<glm::vec3> ring2_positions, ring2_normals;

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				float angle = 2.0f * std::numbers::pi * j / TRAIL_SEGMENTS;
				float cos_angle = cos(angle);
				float sin_angle = sin(angle);

				// Ring 1
				Vector3 offset1 = (normals[i] * cos_angle + binormals[i] * sin_angle) * thickness1;
				Vector3 pos1 = curve_positions[i] + offset1;
				Vector3 norm1 = offset1.Normalized();

				ring1_positions.emplace_back(pos1.x, pos1.y, pos1.z);
				ring1_normals.emplace_back(norm1.x, norm1.y, norm1.z);

				// Ring 2
				Vector3 offset2 = (normals[i + 1] * cos_angle + binormals[i + 1] * sin_angle) * thickness2;
				Vector3 pos2 = curve_positions[i + 1] + offset2;
				Vector3 norm2 = offset2.Normalized();

				ring2_positions.emplace_back(pos2.x, pos2.y, pos2.z);
				ring2_normals.emplace_back(norm2.x, norm2.y, norm2.z);
			}

			// Create triangle strip connecting the two rings
			for (int j = 0; j < TRAIL_SEGMENTS; ++j) {
				// First triangle
				mesh_vertices.push_back(
					{ring1_positions[j],
				     ring1_normals[j],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z)}
				);
				mesh_vertices.push_back(
					{ring2_positions[j],
				     ring2_normals[j],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z)}
				);
				mesh_vertices.push_back(
					{ring1_positions[j + 1],
				     ring1_normals[j + 1],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z)}
				);

				// Second triangle
				mesh_vertices.push_back(
					{ring1_positions[j + 1],
				     ring1_normals[j + 1],
				     glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z)}
				);
				mesh_vertices.push_back(
					{ring2_positions[j],
				     ring2_normals[j],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z)}
				);
				mesh_vertices.push_back(
					{ring2_positions[j + 1],
				     ring2_normals[j + 1],
				     glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z)}
				);
			}
		}

		vertex_count = mesh_vertices.size();
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});
		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
		}

		mesh_dirty = true;
	}

	void Trail::Render(Shader& shader) const {
		if (points.size() < 4) {
			return;
		}

		if (mesh_dirty) {
			const_cast<Trail*>(this)->GenerateMesh();
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

			glBindVertexArray(0);
		}

		shader.use();
		shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count);
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);
	}

} // namespace Boidsish
