#include "trail.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

namespace Boidsish {

	 Trail::Trail(int max_length, float thickness):
		max_length(max_length), thickness(thickness), vertex_count(0), mesh_dirty(false) {
		mesh_vertices.resize(max_length * CURVE_SEGMENTS * VERTS_PER_STEP);
		indices.resize(max_length * CURVE_SEGMENTS * VERTS_PER_STEP);
		for (unsigned int i = 0; i < indices.size(); ++i) { indices[i] = i; }
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
		Vector3 axis = prev_tangent.Cross(curr_tangent);
		float   axis_length = axis.Magnitude();
		if (axis_length < 1e-6f) return prev_normal;
		axis = axis / axis_length;
		float angle = std::acos(std::clamp(prev_tangent.Dot(curr_tangent), -1.0f, 1.0f));
		Vector3 rotated = prev_normal * std::cos(angle) + axis.Cross(prev_normal) * std::sin(angle) +
			axis * (axis.Dot(prev_normal)) * (1.0f - std::cos(angle));
		return rotated.Normalized();
	}

	void Trail::GenerateRing(
		const Vector3& position, const Vector3& normal, const Vector3& binormal,
		std::vector<glm::vec3>& out_positions, std::vector<glm::vec3>& out_normals
	) const {
		out_positions.clear(); out_normals.clear();
		for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
			float   angle = 2.0f * glm::pi<float>() * j / TRAIL_SEGMENTS;
			Vector3 offset = (normal * std::cos(angle) + binormal * std::sin(angle)) * thickness;
			out_positions.emplace_back(position.x + offset.x, position.y + offset.y, position.z + offset.z);
			out_normals.emplace_back(offset.Normalized().x, offset.Normalized().y, offset.Normalized().z);
		}
	}

	void Trail::AppendToGeometryCache(
		const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
		const Vector3& c0, const Vector3& c1, const Vector3& c2, const Vector3& c3
	) {
		size_t start_idx = curve_positions.size();
		for (int j = 0; j < CURVE_SEGMENTS; ++j) {
			float t = (float)j / CURVE_SEGMENTS;
			curve_positions.push_back(CatmullRom(t, p0, p1, p2, p3));
			curve_colors.push_back(CatmullRom(t, c0, c1, c2, c3));
		}

		for (size_t i = start_idx; i < curve_positions.size(); ++i) {
			Vector3 tangent;
			if (i < curve_positions.size() - 1) { tangent = (curve_positions[i + 1] - curve_positions[i]).Normalized(); }
			else if (i > 0) { tangent = (curve_positions[i] - curve_positions[i - 1]).Normalized(); }
			else { tangent = (p2 - p1).Normalized(); }
			tangents.push_back(tangent);

			Vector3 normal;
			if (normals.empty()) {
				if (std::abs(tangent.y) < 0.999f) normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				else normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
			} else {
				normal = TransportFrame(normals.back(), tangents[tangents.size()-2], tangent);
			}
			normals.push_back(normal);
			binormals.push_back(tangent.Cross(normal).Normalized());

			std::vector<glm::vec3> rpos, rnorm;
			GenerateRing(curve_positions[i], normal, binormals.back(), rpos, rnorm);
			ring_positions.push_back(rpos); ring_normals.push_back(rnorm);
		}

		// Connect newly added rings
		size_t num_rings = ring_positions.size();
		size_t first_new_ring = num_rings - CURVE_SEGMENTS;
		size_t start_tri_idx = (first_new_ring > 0) ? first_new_ring - 1 : 0;

		for (size_t i = start_tri_idx; i < num_rings - 1; ++i) {
			const auto& r1p = ring_positions[i]; const auto& r1n = ring_normals[i];
			const auto& r2p = ring_positions[i+1]; const auto& r2n = ring_normals[i+1];
			const auto& col = curve_colors[i];
			for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
				mesh_vertices[tail] = {r2p[j], r2n[j], glm::vec3(col.x, col.y, col.z)};
				tail = (tail + 1) % mesh_vertices.size();
				mesh_vertices[tail] = {r1p[j], r1n[j], glm::vec3(col.x, col.y, col.z)};
				tail = (tail + 1) % mesh_vertices.size();
			}
		}
	}

	void Trail::UpdateMesh() {
		size_t num_finalized_segments = curve_positions.size() / CURVE_SEGMENTS;
		size_t total_finalizable = (points.size() >= 3) ? points.size() - 2 : 0;
		tail = permanent_tail;

		while (num_finalized_segments < total_finalizable) {
			size_t i = num_finalized_segments;
			Vector3 p0, p1, p2, p3; Vector3 c0, c1, c2, c3;
			p1 = Vector3(points[i].first); p2 = Vector3(points[i+1].first); p3 = Vector3(points[i+2].first);
			c1 = Vector3(points[i].second); c2 = Vector3(points[i+1].second); c3 = Vector3(points[i+2].second);
			if (i == 0) { p0 = p1 - (p2 - p1); c0 = c1; }
			else { p0 = Vector3(points[i - 1].first); c0 = Vector3(points[i - 1].second); }
			AppendToGeometryCache(p0, p1, p2, p3, c0, c1, c2, c3);
			permanent_tail = tail; num_finalized_segments++;
		}

		if (points.size() >= 2) {
			std::vector<Vector3> temp_curve; std::vector<Vector3> temp_colors;
			for (size_t i = num_finalized_segments; i < points.size() - 1; ++i) {
				Vector3 p0, p1, p2, p3; Vector3 c0, c1, c2, c3;
				p1 = Vector3(points[i].first); p2 = Vector3(points[i+1].first);
				c1 = Vector3(points[i].second); c2 = Vector3(points[i+1].second);
				if (i == 0) { p0 = p1 - (p2 - p1); c0 = c1; }
				else { p0 = Vector3(points[i - 1].first); c0 = Vector3(points[i - 1].second); }
				p3 = p2 + (p2 - p1); c3 = c2;
				for (int j = 0; j < CURVE_SEGMENTS; ++j) {
					float t = (float)j / CURVE_SEGMENTS;
					temp_curve.push_back(CatmullRom(t, p0, p1, p2, p3));
					temp_colors.push_back(CatmullRom(t, c0, c1, c2, c3));
				}
				if (i == points.size() - 2) { temp_curve.push_back(p2); temp_colors.push_back(c2); }
			}

			Vector3 last_tangent = tangents.empty() ? Vector3(0,0,0) : tangents.back();
			Vector3 last_normal = normals.empty() ? Vector3(0,1,0) : normals.back();
			std::vector<glm::vec3> prev_rpos, prev_rnorm;
			if (!ring_positions.empty()) { prev_rpos = ring_positions.back(); prev_rnorm = ring_normals.back(); }

			for (size_t i = 0; i < temp_curve.size(); ++i) {
				Vector3 tangent;
				if (i == 0 && last_tangent.Magnitude() < 1e-6f) { tangent = (temp_curve.size() > 1) ? (temp_curve[1] - temp_curve[0]).Normalized() : Vector3(0,0,1); }
				else if (i == 0) { tangent = last_tangent; }
				else { tangent = (temp_curve[i] - temp_curve[i-1]).Normalized(); }

				Vector3 normal = (i == 0 && normals.empty()) ? (std::abs(tangent.y) < 0.999f ? tangent.Cross(Vector3(0, 1, 0)).Normalized() : tangent.Cross(Vector3(1, 0, 0)).Normalized())
				                                            : TransportFrame(last_normal, last_tangent, tangent);
				Vector3 binormal = tangent.Cross(normal).Normalized();
				std::vector<glm::vec3> curr_rpos, curr_rnorm;
				GenerateRing(temp_curve[i], normal, binormal, curr_rpos, curr_rnorm);

				if (!prev_rpos.empty()) {
					const auto& col = (i == 0) ? (curve_colors.empty() ? temp_colors[0] : curve_colors.back()) : temp_colors[i-1];
					for (int j = 0; j <= TRAIL_SEGMENTS; ++j) {
						mesh_vertices[tail] = {curr_rpos[j], curr_rnorm[j], glm::vec3(col.x, col.y, col.z)};
						tail = (tail + 1) % mesh_vertices.size();
						mesh_vertices[tail] = {prev_rpos[j], prev_rnorm[j], glm::vec3(col.x, col.y, col.z)};
						tail = (tail + 1) % mesh_vertices.size();
					}
				}
				prev_rpos = curr_rpos; prev_rnorm = curr_rnorm; last_tangent = tangent; last_normal = normal;
			}
		}
		if (full) vertex_count = mesh_vertices.size();
		else if (tail >= head) vertex_count = tail - head;
		else vertex_count = mesh_vertices.size() - head + tail;
	}

	void Trail::PopFromGeometryCache() {
		if (curve_positions.empty()) return;
		for (int i = 0; i < CURVE_SEGMENTS; ++i) {
			curve_positions.pop_front(); curve_colors.pop_front(); tangents.pop_front();
			normals.pop_front(); binormals.pop_front(); ring_positions.pop_front(); ring_normals.pop_front();
		}
		head = (head + CURVE_SEGMENTS * VERTS_PER_STEP) % mesh_vertices.size();
		full = false;
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});
		if (points.size() > static_cast<size_t>(max_length)) { points.pop_front(); PopFromGeometryCache(); }
		UpdateMesh(); mesh_dirty = true;
	}

	void Trail::SetIridescence(bool enabled) { iridescent_ = enabled; }
	void Trail::SetUseRocketTrail(bool enabled) { useRocketTrail_ = enabled; }
	std::vector<float> Trail::GetInterleavedVertexData() const {
		std::vector<float> data; data.reserve(mesh_vertices.size() * 9);
		for (const auto& v : mesh_vertices) {
			data.push_back(v.pos.x); data.push_back(v.pos.y); data.push_back(v.pos.z);
			data.push_back(v.normal.x); data.push_back(v.normal.y); data.push_back(v.normal.z);
			data.push_back(v.color.x); data.push_back(v.color.y); data.push_back(v.color.z);
		}
		return data;
	}

} // namespace Boidsish
