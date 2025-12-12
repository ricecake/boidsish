#include "trail.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Boidsish {

Trail::Trail(int max_length) :
	max_length(max_length), vertex_count(0), mesh_dirty(false) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);

	// Normal
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, normal)
	);

	// Color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		2, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color)
	);

	glBindVertexArray(0);
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
		if (points.size() < 4) {
			mesh_vertices.clear();
			curve_positions.clear();
			curve_colors.clear();
			tangents.clear();
			normals.clear();
			binormals.clear();
			vertex_count = 0;
			return;
		}

		// Full rebuild if points were removed (e.g. trail shortened)
		if (curve_positions.size() > points.size() * CURVE_SEGMENTS) {
			mesh_vertices.clear();
			curve_positions.clear();
			curve_colors.clear();
			tangents.clear();
			normals.clear();
			binormals.clear();
		}

		std::vector<Vector3> positions;
		std::vector<Vector3> colors;

		for (const auto& point : points) {
			positions.emplace_back(point.first.x, point.first.y, point.first.z);
			colors.emplace_back(point.second.x, point.second.y, point.second.z);
		}

		// Generate smooth curve using Catmull-Rom splines for new points
		size_t start_point_idx = curve_positions.empty() ? 0 : (curve_positions.size() / CURVE_SEGMENTS);
		if (start_point_idx > 0)
			start_point_idx--; // Start one point back to ensure continuity

		for (size_t i = start_point_idx; i < positions.size() - 3; ++i) {
			for (int j = 0; j < CURVE_SEGMENTS; ++j) {
				float   t = (float)j / CURVE_SEGMENTS;
				Vector3 pos = CatmullRom(t, positions[i], positions[i + 1], positions[i + 2], positions[i + 3]);
				Vector3 color = CatmullRom(t, colors[i], colors[i + 1], colors[i + 2], colors[i + 3]);

				if (i * CURVE_SEGMENTS + j >= curve_positions.size()) {
					curve_positions.push_back(pos);
					curve_colors.push_back(color);
				} else {
					// Overwrite existing points to ensure smooth transition
					curve_positions[i * CURVE_SEGMENTS + j] = pos;
					curve_colors[i * CURVE_SEGMENTS + j] = color;
				}
			}
		}

		// Add the final point
		curve_positions.push_back(positions[positions.size() - 2]);
		curve_colors.push_back(colors[colors.size() - 2]);

		if (curve_positions.size() < 2) {
			vertex_count = 0;
			return;
		}

		// Generate tangents and frame transport
		size_t tangent_start_idx = tangents.empty() ? 0 : tangents.size() - 1;
		for (size_t i = tangent_start_idx; i < curve_positions.size(); ++i) {
			Vector3 tangent;
			if (i == 0) {
				tangent = (curve_positions[i + 1] - curve_positions[i]).Normalized();
			} else if (i == curve_positions.size() - 1) {
				tangent = (curve_positions[i] - curve_positions[i - 1]).Normalized();
			} else {
				tangent = (curve_positions[i + 1] - curve_positions[i - 1]).Normalized();
			}

			if (i >= tangents.size()) {
				tangents.push_back(tangent);
			} else {
				tangents[i] = tangent;
			}
		}

		// Initialize first frame or transport from previous
		size_t frame_start_idx = normals.empty() ? 0 : normals.size() - 1;
		if (frame_start_idx == 0) {
			Vector3 initial_normal;
			if (abs(tangents[0].y) < 0.999f) {
				initial_normal = tangents[0].Cross(Vector3(0, 1, 0)).Normalized();
			} else {
				initial_normal = tangents[0].Cross(Vector3(1, 0, 0)).Normalized();
			}
			normals.push_back(initial_normal);
			binormals.push_back(tangents[0].Cross(initial_normal).Normalized());
			frame_start_idx = 1;
		}

		// Transport frame along the curve
		for (size_t i = frame_start_idx; i < curve_positions.size(); ++i) {
			Vector3 transported_normal = TransportFrame(normals[i - 1], tangents[i - 1], tangents[i]);
			if (i >= normals.size()) {
				normals.push_back(transported_normal);
				binormals.push_back(tangents[i].Cross(transported_normal).Normalized());
			} else {
				normals[i] = transported_normal;
				binormals[i] = tangents[i].Cross(transported_normal).Normalized();
			}
		}

		// Generate cylindrical mesh for new segments
		size_t mesh_start_idx = (mesh_vertices.size() / (TRAIL_SEGMENTS * 6));
		if (mesh_start_idx > 0)
			mesh_start_idx--;

		for (size_t i = mesh_start_idx; i < curve_positions.size() - 1; ++i) {
			float progress1 = (float)i / (curve_positions.size() - 1);
			float thickness1 = BASE_THICKNESS * (0.2f + progress1 * 0.8f);
			float progress2 = (float)(i + 1) / (curve_positions.size() - 1);
			float thickness2 = BASE_THICKNESS * (0.2f + progress2 * 0.8f);

			std::vector<TrailVertex> new_vertices;
			for (int j = 0; j < TRAIL_SEGMENTS; ++j) {
				float angle1 = 2.0f * std::numbers::pi * j / TRAIL_SEGMENTS;
				float angle2 = 2.0f * std::numbers::pi * (j + 1) / TRAIL_SEGMENTS;

				Vector3 offset1a = (normals[i] * cos(angle1) + binormals[i] * sin(angle1)) * thickness1;
				Vector3 pos1a = curve_positions[i] + offset1a;
				Vector3 norm1a = offset1a.Normalized();
				Vector3 offset1b = (normals[i] * cos(angle2) + binormals[i] * sin(angle2)) * thickness1;
				Vector3 pos1b = curve_positions[i] + offset1b;
				Vector3 norm1b = offset1b.Normalized();

				Vector3 offset2a = (normals[i + 1] * cos(angle1) + binormals[i + 1] * sin(angle1)) * thickness2;
				Vector3 pos2a = curve_positions[i + 1] + offset2a;
				Vector3 norm2a = offset2a.Normalized();
				Vector3 offset2b = (normals[i + 1] * cos(angle2) + binormals[i + 1] * sin(angle2)) * thickness2;
				Vector3 pos2b = curve_positions[i + 1] + offset2b;
				Vector3 norm2b = offset2b.Normalized();

				glm::vec3 color1(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z);
				glm::vec3 color2(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z);

				new_vertices.push_back({{pos1a.x, pos1a.y, pos1a.z}, {norm1a.x, norm1a.y, norm1a.z}, color1});
				new_vertices.push_back({{pos2a.x, pos2a.y, pos2a.z}, {norm2a.x, norm2a.y, norm2a.z}, color2});
				new_vertices.push_back({{pos1b.x, pos1b.y, pos1b.z}, {norm1b.x, norm1b.y, norm1b.z}, color1});
				new_vertices.push_back({{pos1b.x, pos1b.y, pos1b.z}, {norm1b.x, norm1b.y, norm1b.z}, color1});
				new_vertices.push_back({{pos2a.x, pos2a.y, pos2a.z}, {norm2a.x, norm2a.y, norm2a.z}, color2});
				new_vertices.push_back({{pos2b.x, pos2b.y, pos2b.z}, {norm2b.x, norm2b.y, norm2b.z}, color2});
			}

			size_t replace_idx = i * TRAIL_SEGMENTS * 6;
			for (size_t k = 0; k < new_vertices.size(); ++k) {
				if (replace_idx + k < mesh_vertices.size()) {
					mesh_vertices[replace_idx + k] = new_vertices[k];
				} else {
					mesh_vertices.push_back(new_vertices[k]);
				}
			}
		}

		// Trim old data if trail is too long
		int max_curve_points = (max_length - 3) * CURVE_SEGMENTS;
		while ((int)curve_positions.size() > max_curve_points) {
			curve_positions.pop_front();
			curve_colors.pop_front();
			tangents.pop_front();
			normals.pop_front();
			binormals.pop_front();
		}

		int max_vertices = (curve_positions.size() - 1) * TRAIL_SEGMENTS * 6;
		if ((int)mesh_vertices.size() > max_vertices) {
			mesh_vertices.erase(
				mesh_vertices.begin(), mesh_vertices.begin() + (mesh_vertices.size() - max_vertices)
			);
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

void Trail::Render(Shader& shader) {
		if (mesh_dirty) {
			GenerateMesh();
			mesh_dirty = false;

			if (vertex_count > 0) {
				// Upload mesh to GPU
				glBindVertexArray(vao);
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(
					GL_ARRAY_BUFFER,
					mesh_vertices.size() * sizeof(TrailVertex),
					mesh_vertices.data(),
					GL_DYNAMIC_DRAW
				);
				glBindVertexArray(0);
			}
		}

		if (vertex_count == 0) {
			return;
		}

		shader.use();
		shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertex_count);
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);
	}

} // namespace Boidsish
