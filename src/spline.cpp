#include "spline.h"

#include <numbers>

namespace Boidsish {
	namespace Spline {

		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
			return 0.5f *
				((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
			     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
		}

		const int   CYLINDER_SEGMENTS = 12;
		const float EDGE_RADIUS_SCALE = 0.005f;
		const int   CURVE_SEGMENTS = 10;

		std::vector<VertexData> GenerateTube(
			const std::vector<Vector3>&   points,
			const std::vector<Vector3>&   ups,
			const std::vector<float>&     sizes,
			const std::vector<glm::vec3>& colors,
			bool                          is_looping
		) {
			std::vector<VertexData> all_vertices_data;
			if (points.size() < 2)
				return all_vertices_data;

			for (size_t i = 0; i < points.size() - 1; ++i) {
				const auto& p1 = points[i];
				const auto& p2 = points[i + 1];

				Vector3 p0, p3;

				if (i > 0) {
					p0 = points[i - 1];
				} else if (is_looping) {
					p0 = points.back();
				} else {
					p0 = p1 - (p2 - p1);
				}

				if (i < points.size() - 2) {
					p3 = points[i + 2];
				} else if (is_looping) {
					p3 = points.front();
				} else {
					p3 = p2 + (p2 - p1);
				}

				std::vector<std::vector<VertexData>> rings;
				Vector3                              last_normal = ups[i];

				for (int j = 0; j <= CURVE_SEGMENTS; ++j) {
					std::vector<VertexData> ring;
					float                   t = (float)j / CURVE_SEGMENTS;

					Vector3   point = CatmullRom(t, p0, p1, p2, p3);
					glm::vec3 color = (1 - t) * colors[i] + t * colors[i + 1];
					float     r = ((1 - t) * sizes[i] + t * sizes[i + 1]) * EDGE_RADIUS_SCALE;

					Vector3 tangent;
					if (j < CURVE_SEGMENTS) {
						tangent = (CatmullRom((float)(j + 1) / CURVE_SEGMENTS, p0, p1, p2, p3) - point).Normalized();
					} else {
						tangent = (point - CatmullRom((float)(j - 1) / CURVE_SEGMENTS, p0, p1, p2, p3)).Normalized();
					}

					Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
					if (normal.MagnitudeSquared() < 1e-6) {
						if (abs(tangent.y) < 0.999)
							normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
						else
							normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
					} else {
						normal.Normalize();
					}
					Vector3 bitangent = tangent.Cross(normal).Normalized();
					last_normal = normal;

					for (int k = 0; k <= CYLINDER_SEGMENTS; ++k) {
						float   angle = 2.0f * std::numbers::pi * k / CYLINDER_SEGMENTS;
						Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
						Vector3 pos = point + cn * r;
						ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
					}
					rings.push_back(ring);
				}

				for (int j = 0; j < CURVE_SEGMENTS; ++j) {
					for (int k = 0; k < CYLINDER_SEGMENTS; ++k) {
						all_vertices_data.push_back(rings[j][k]);
						all_vertices_data.push_back(rings[j][k + 1]);
						all_vertices_data.push_back(rings[j + 1][k]);

						all_vertices_data.push_back(rings[j][k + 1]);
						all_vertices_data.push_back(rings[j + 1][k + 1]);
						all_vertices_data.push_back(rings[j + 1][k]);
					}
				}
			}
			return all_vertices_data;
		}

	} // namespace Spline
} // namespace Boidsish
