#include "trail.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	Trail::Trail(int max_length, float thickness):
		max_length(max_length), thickness(thickness), vertex_count(0), mesh_dirty(false) {
		// Pre-allocate mesh data for TrailRenderManager
		mesh_vertices.resize(max_length * CURVE_SEGMENTS * VERTS_PER_STEP);
		indices.resize(max_length * CURVE_SEGMENTS * VERTS_PER_STEP);
		for (unsigned int i = 0; i < indices.size(); ++i) {
			indices[i] = i;
		}
	}

	Trail::~Trail() = default;

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

	void Trail::UpdateAndAppendSegment() {
		// Get the last 4 points to calculate the new segment
		int                  size = points.size();
		std::vector<Vector3> p(4);
		std::vector<Vector3> c(4);
		for (int i = 0; i < 4; ++i) {
			p[i] = Vector3(points[size - 4 + i].first.x, points[size - 4 + i].first.y, points[size - 4 + i].first.z);
			c[i] = Vector3(points[size - 4 + i].second.x, points[size - 4 + i].second.y, points[size - 4 + i].second.z);
		}

		// Append to the geometry cache
		AppendToGeometryCache(p[0], p[1], p[2], p[3], c[0], c[1], c[2], c[3]);

		// Calculate tangents, normals, and binormals for the new segment
		size_t start_index = curve_positions.size() - CURVE_SEGMENTS;
		for (size_t i = start_index; i < curve_positions.size(); ++i) {
			Vector3 tangent;
			if (i == 0) {
				tangent = (curve_positions[i + 1] - curve_positions[i]).Normalized();
			} else {
				tangent = (curve_positions[i] - curve_positions[i - 1]).Normalized();
			}
			tangents.push_back(tangent);

			Vector3 normal;
			if (i == 0) {
				if (abs(tangents[i].y) < 0.999f) {
					normal = tangents[i].Cross(Vector3(0, 1, 0)).Normalized();
				} else {
					normal = tangents[i].Cross(Vector3(1, 0, 0)).Normalized();
				}
			} else {
				normal = TransportFrame(normals.back(), tangents[i - 1], tangents[i]);
			}
			normals.push_back(normal);
			binormals.push_back(tangents[i].Cross(normal).Normalized());
		}

		// Generate rings for the new segment
		size_t ring_start_index = curve_positions.size() - CURVE_SEGMENTS;
		for (size_t i = ring_start_index; i < curve_positions.size(); ++i) {
			std::vector<glm::vec3> current_ring_pos;
			std::vector<glm::vec3> current_ring_norm;
			// float                  thickness = BASE_THICKNESS;

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				float   angle = 2.0f * glm::pi<float>() * j / TRAIL_SEGMENTS;
				Vector3 offset = (normals[i] * cos(angle) + binormals[i] * sin(angle)) * thickness;
				Vector3 pos = curve_positions[i] + offset;
				Vector3 norm = offset.Normalized();
				current_ring_pos.emplace_back(pos.x, pos.y, pos.z);
				current_ring_norm.emplace_back(norm.x, norm.y, norm.z);
			}
			ring_positions.push_back(current_ring_pos);
			ring_normals.push_back(current_ring_norm);
		}

		// Generate the mesh for the new segment
		size_t segment_start_index = (curve_positions.size() == CURVE_SEGMENTS)
			? 0
			: curve_positions.size() - CURVE_SEGMENTS - 1;
		for (size_t i = segment_start_index; i < curve_positions.size() - 1; ++i) {
			const auto& ring1_positions = ring_positions[i];
			const auto& ring1_normals = ring_normals[i];
			const auto& ring2_positions = ring_positions[i + 1];
			const auto& ring2_normals = ring_normals[i + 1];

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				int idx = j % (TRAIL_SEGMENTS + 1);

				// Vertex from the CURRENT ring
				mesh_vertices[tail] = {
					ring2_positions[idx],
					ring2_normals[idx],
					glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				};
				tail = (tail + 1) % mesh_vertices.size();

				// Vertex from the PREVIOUS ring
				mesh_vertices[tail] = {
					ring1_positions[idx],
					ring1_normals[idx],
					glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				};
				tail = (tail + 1) % mesh_vertices.size();
			}
		}
		if (full) {
			vertex_count = mesh_vertices.size();
		} else if (tail > head) {
			vertex_count = tail - head;
		} else {
			vertex_count = mesh_vertices.size() - head + tail;
		}
		if (tail == head && points.size() > 4) {
			full = true;
		}
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});

		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
			PopFromGeometryCache();
		}

		if (points.size() >= 4) {
			UpdateAndAppendSegment();
		}

		mesh_dirty = true;
	}

	void Trail::SetIridescence(bool enabled) {
		iridescent_ = enabled;
	}

	void Trail::SetUseRocketTrail(bool enabled) {
		useRocketTrail_ = enabled;
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
			tangents.pop_front();
			normals.pop_front();
			binormals.pop_front();
			ring_positions.pop_front();
			ring_normals.pop_front();
		}
		head = (head + CURVE_SEGMENTS * VERTS_PER_STEP) % mesh_vertices.size();
		full = false;
	}

	std::vector<float> Trail::GetInterleavedVertexData() const {
		// Convert TrailVertex array to interleaved float array
		// Format: [pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, color.x, color.y, color.z] per vertex
		std::vector<float> data;
		data.reserve(mesh_vertices.size() * 9);

		for (const auto& v : mesh_vertices) {
			data.push_back(v.pos.x);
			data.push_back(v.pos.y);
			data.push_back(v.pos.z);
			data.push_back(v.normal.x);
			data.push_back(v.normal.y);
			data.push_back(v.normal.z);
			data.push_back(v.color.x);
			data.push_back(v.color.y);
			data.push_back(v.color.z);
		}

		return data;
	}

} // namespace Boidsish
