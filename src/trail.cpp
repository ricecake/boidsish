#include "trail.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

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
		float angle = std::acos(std::clamp(prev_tangent.Dot(curr_tangent), -1.0f, 1.0f));

		// Rodrigues' rotation formula
		Vector3 rotated = prev_normal * std::cos(angle) + axis.Cross(prev_normal) * std::sin(angle) +
			axis * (axis.Dot(prev_normal)) * (1.0f - std::cos(angle));

		return rotated.Normalized();
	}

	void Trail::UpdateMesh() {
		// Regenerate the entire trail mesh from scratch to ensure no gaps or lag.
		// This provides immediate connection to the emitter and smooth Catmull-Rom curves.
		curve_positions.clear();
		curve_colors.clear();
		tangents.clear();
		normals.clear();
		binormals.clear();
		ring_positions.clear();
		ring_normals.clear();
		head = 0;
		tail = 0;
		full = false;
		vertex_count = 0;

		if (points.size() < 2)
			return;

		// 1. Generate interpolated curve positions and colors
		for (size_t i = 0; i < points.size() - 1; ++i) {
			Vector3 p0, p1, p2, p3;
			Vector3 c0, c1, c2, c3;

			p1 = Vector3(points[i].first.x, points[i].first.y, points[i].first.z);
			p2 = Vector3(points[i + 1].first.x, points[i + 1].first.y, points[i + 1].first.z);
			c1 = Vector3(points[i].second.x, points[i].second.y, points[i].second.z);
			c2 = Vector3(points[i + 1].second.x, points[i + 1].second.y, points[i + 1].second.z);

			// Extrapolate control points at the start and end of the trail
			if (i == 0) {
				p0 = p1 - (p2 - p1);
				c0 = c1;
			} else {
				p0 = Vector3(points[i - 1].first.x, points[i - 1].first.y, points[i - 1].first.z);
				c0 = Vector3(points[i - 1].second.x, points[i - 1].second.y, points[i - 1].second.z);
			}

			if (i + 1 == points.size() - 1) {
				p3 = p2 + (p2 - p1);
				c3 = c2;
			} else {
				p3 = Vector3(points[i + 2].first.x, points[i + 2].first.y, points[i + 2].first.z);
				c3 = Vector3(points[i + 2].second.x, points[i + 2].second.y, points[i + 2].second.z);
			}

			for (int j = 0; j < CURVE_SEGMENTS; ++j) {
				float   t = (float)j / CURVE_SEGMENTS;
				curve_positions.push_back(CatmullRom(t, p0, p1, p2, p3));
				curve_colors.push_back(CatmullRom(t, c0, c1, c2, c3));
			}

			// Add the final point of the trail
			if (i == points.size() - 2) {
				curve_positions.push_back(p2);
				curve_colors.push_back(c2);
			}
		}

		// 2. Calculate tangents and propagate orientation (Frame Transport)
		for (size_t i = 0; i < curve_positions.size(); ++i) {
			Vector3 tangent;
			if (i < curve_positions.size() - 1) {
				tangent = (curve_positions[i + 1] - curve_positions[i]).Normalized();
			} else {
				tangent = (curve_positions[i] - curve_positions[i - 1]).Normalized();
			}
			tangents.push_back(tangent);

			Vector3 normal;
			if (i == 0) {
				// Establish initial normal orthogonal to the first tangent
				if (std::abs(tangent.y) < 0.999f) {
					normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				} else {
					normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				}
			} else {
				normal = TransportFrame(normals.back(), tangents[i - 1], tangents[i]);
			}
			normals.push_back(normal);
			binormals.push_back(tangent.Cross(normal).Normalized());
		}

		// 3. Generate rings for each curve point
		for (size_t i = 0; i < curve_positions.size(); ++i) {
			std::vector<glm::vec3> current_ring_pos;
			std::vector<glm::vec3> current_ring_norm;

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				float   angle = 2.0f * glm::pi<float>() * j / TRAIL_SEGMENTS;
				Vector3 offset = (normals[i] * std::cos(angle) + binormals[i] * std::sin(angle)) * thickness;
				Vector3 pos = curve_positions[i] + offset;
				Vector3 norm = offset.Normalized();
				current_ring_pos.emplace_back(pos.x, pos.y, pos.z);
				current_ring_norm.emplace_back(norm.x, norm.y, norm.z);
			}
			ring_positions.push_back(current_ring_pos);
			ring_normals.push_back(current_ring_norm);
		}

		// 4. Triangulate segments between rings into the interleaved vertex buffer
		for (size_t i = 0; i < ring_positions.size() - 1; ++i) {
			const auto& ring1_positions = ring_positions[i];
			const auto& ring1_normals = ring_normals[i];
			const auto& ring2_positions = ring_positions[i + 1];
			const auto& ring2_normals = ring_normals[i + 1];

			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				// CCW Triangle Strip winding: alternate between the two rings
				mesh_vertices[tail++] = {
					ring2_positions[j],
					ring2_normals[j],
					glm::vec3(curve_colors[i + 1].x, curve_colors[i + 1].y, curve_colors[i + 1].z),
				};
				mesh_vertices[tail++] = {
					ring1_positions[j],
					ring1_normals[j],
					glm::vec3(curve_colors[i].x, curve_colors[i].y, curve_colors[i].z),
				};
			}
		}

		vertex_count = static_cast<int>(tail);
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});

		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
		}

		UpdateMesh();
		mesh_dirty = true;
	}

	void Trail::SetIridescence(bool enabled) {
		iridescent_ = enabled;
	}

	void Trail::SetUseRocketTrail(bool enabled) {
		useRocketTrail_ = enabled;
	}

	std::vector<float> Trail::GetInterleavedVertexData() const {
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
