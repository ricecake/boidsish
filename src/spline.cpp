#include "spline.h"

#include <map>
#include <numbers>

namespace Boidsish {
	namespace Spline {

		Vector3 CatmullRom(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
			return 0.5f *
				((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
			     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
		}

		Vector3
		CatmullRomDerivative(float t, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
			return 0.5f *
				((-p0 + p2) + 2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t +
			     3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t));
		}

		const float EDGE_RADIUS_SCALE = 0.005f;

		std::vector<VertexData> GenerateGraphTube(
			const std::vector<GraphNode>& nodes,
			const std::vector<GraphEdge>& edges,
			int                           curve_segments,
			int                           cylinder_segments
		) {
			std::vector<VertexData> all_vertices_data;
			if (edges.empty())
				return all_vertices_data;

			std::map<int, std::vector<int>> adj;
			for (const auto& edge : edges) {
				adj[edge.from].push_back(edge.to);
				adj[edge.to].push_back(edge.from);
			}

			for (const auto& edge : edges) {
				if (edge.from >= (int)nodes.size() || edge.to >= (int)nodes.size())
					continue;

				const auto& v1 = nodes[edge.from];
				const auto& v2 = nodes[edge.to];

				Vector3 p1 = v1.position;
				Vector3 p2 = v2.position;

				Vector3 p0 = p1;
				if (adj.count(edge.from)) {
					for (int n_idx : adj.at(edge.from)) {
						if (n_idx != edge.to) {
							p0 = nodes[n_idx].position;
							break;
						}
					}
				}
				if ((p0 - p1).MagnitudeSquared() < 1e-9)
					p0 = p1 - (p2 - p1);

				Vector3 p3 = p2;
				if (adj.count(edge.to)) {
					for (int n_idx : adj.at(edge.to)) {
						if (n_idx != edge.from) {
							p3 = nodes[n_idx].position;
							break;
						}
					}
				}
				if ((p3 - p2).MagnitudeSquared() < 1e-9)
					p3 = p2 + (p2 - p1);

				std::vector<std::vector<VertexData>> rings;
				Vector3                              last_normal;

				{
					Vector3 point1 = CatmullRom(0.0f, p0, p1, p2, p3);
					Vector3 point2 = CatmullRom(1.0f / curve_segments, p0, p1, p2, p3);
					Vector3 tangent;
					if ((point2 - point1).MagnitudeSquared() < 1e-6) {
						tangent = Vector3(0, 1, 0);
					} else {
						tangent = (point2 - point1).Normalized();
					}

					if (abs(tangent.y) < 0.999)
						last_normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
					else
						last_normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				}

				for (int i = 0; i <= curve_segments; ++i) {
					std::vector<VertexData> ring;
					float                   t = (float)i / curve_segments;

					Vector3   point = CatmullRom(t, p0, p1, p2, p3);
					glm::vec3 color = (1 - t) * v1.color + t * v2.color;
					float     r = ((1 - t) * v1.size + t * v2.size) * EDGE_RADIUS_SCALE;

					Vector3 tangent;
					if (i < curve_segments) {
						Vector3 next_point = CatmullRom((float)(i + 1) / curve_segments, p0, p1, p2, p3);
						if ((next_point - point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (next_point - point).Normalized();
						}
					} else {
						Vector3 prev_point = CatmullRom((float)(i - 1) / curve_segments, p0, p1, p2, p3);
						if ((point - prev_point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (point - prev_point).Normalized();
						}
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

					for (int j = 0; j <= cylinder_segments; ++j) {
						float   angle = 2.0f * std::numbers::pi * j / cylinder_segments;
						Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
						Vector3 pos = point + cn * r;
						ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
					}
					rings.push_back(ring);
				}

				for (int i = 0; i < curve_segments; ++i) {
					for (int j = 0; j < cylinder_segments; ++j) {
						all_vertices_data.push_back(rings[i][j]);
						all_vertices_data.push_back(rings[i][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j]);

						all_vertices_data.push_back(rings[i][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j]);
					}
				}
			}
			return all_vertices_data;
		}

		std::vector<VertexData> GenerateTube(
			const std::vector<Vector3>&   points,
			const std::vector<Vector3>&   ups,
			const std::vector<float>&     sizes,
			const std::vector<glm::vec3>& colors,
			bool                          is_looping,
			int                           curve_segments,
			int                           cylinder_segments
		) {
			std::vector<VertexData> all_vertices_data;
			if (points.size() < 2)
				return all_vertices_data;

			for (size_t i = 0; i < points.size() - (is_looping ? 0 : 1); ++i) {
				int p0_idx = (i == 0) ? (is_looping ? points.size() - 1 : 0) : i - 1;
				int p1_idx = i;
				int p2_idx = (i + 1) % points.size();
				int p3_idx = (i + 2) % points.size();

				const auto& p0 = (i == 0 && !is_looping) ? points[p1_idx] - (points[p2_idx] - points[p1_idx])
														 : points[p0_idx];
				const auto& p1 = points[p1_idx];
				const auto& p2 = points[p2_idx];
				const auto& p3 = (i >= points.size() - 2 && !is_looping)
					? points[p2_idx] + (points[p2_idx] - points[p1_idx])
					: points[p3_idx];

				std::vector<std::vector<VertexData>> rings;
				Vector3                              last_normal = ups[p1_idx];

				for (int j = 0; j <= curve_segments; ++j) {
					std::vector<VertexData> ring;
					float                   t = (float)j / curve_segments;

					Vector3   point = CatmullRom(t, p0, p1, p2, p3);
					glm::vec3 color = (1 - t) * colors[p1_idx] + t * colors[p2_idx];
					float     r = ((1 - t) * sizes[p1_idx] + t * sizes[p2_idx]) * EDGE_RADIUS_SCALE;

					Vector3 tangent;
					if (j < curve_segments) {
						Vector3 next_point = CatmullRom((float)(j + 1) / curve_segments, p0, p1, p2, p3);
						if ((next_point - point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (next_point - point).Normalized();
						}
					} else {
						Vector3 prev_point = CatmullRom((float)(j - 1) / curve_segments, p0, p1, p2, p3);
						if ((point - prev_point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (point - prev_point).Normalized();
						}
					}

					Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
					if (normal.MagnitudeSquared() < 1e-6) {
						if (std::abs(tangent.y) < 0.999)
							normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
						else
							normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
					} else {
						normal.Normalize();
					}
					Vector3 bitangent = tangent.Cross(normal).Normalized();
					last_normal = normal;

					for (int k = 0; k <= cylinder_segments; ++k) {
						float   angle = 2.0f * std::numbers::pi * k / cylinder_segments;
						Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
						Vector3 pos = point + cn * r;
						ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
					}
					rings.push_back(ring);
				}

				for (int j = 0; j < curve_segments; ++j) {
					for (int k = 0; k < cylinder_segments; ++k) {
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
