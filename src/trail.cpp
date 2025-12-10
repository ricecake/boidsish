#include "trail.h"

#include <algorithm>
#include <numbers>
#include <vector>

#include "vector.h"

namespace Boidsish {

	const int CYLINDER_SEGMENTS = 8;
	const float RADIUS = 0.05f;

	struct TrailVertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 color;
		float     progress;
	};

	Trail::Trail(int max_length): max_length(max_length), vertex_count(0) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		// Pre-allocate buffer
		glBufferData(
			GL_ARRAY_BUFFER,
			max_length * (CYLINDER_SEGMENTS + 1) * 2 * sizeof(TrailVertex),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		// Position
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, pos));

		// Normal
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, normal));

		// Color
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));

		// Progress
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(
			3, 1, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, progress)
		);

		glBindVertexArray(0);
	}

	Trail::~Trail() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}

	void Trail::AddPoint(glm::vec3 position, glm::vec3 color) {
		points.push_back({position, color});
		if (points.size() > static_cast<size_t>(max_length)) {
			points.pop_front();
		}

		if (points.size() < 2) {
			vertex_count = 0;
			return;
		}

		std::vector<TrailVertex> vertices_data;
		Vector3                  last_normal;

		// Initial frame
		{
			Vector3 tangent =
				(Vector3(points[1].first.x, points[1].first.y, points[1].first.z) -
			     Vector3(points[0].first.x, points[0].first.y, points[0].first.z))
					.Normalized();
			if (abs(tangent.y) < 0.999) {
				last_normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
			} else {
				last_normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
			}
		}

		std::vector<std::vector<TrailVertex>> rings;
		for (size_t i = 0; i < points.size(); ++i) {
			std::vector<TrailVertex> ring;
			Vector3                  point(points[i].first.x, points[i].first.y, points[i].first.z);
			glm::vec3                  v_color = points[i].second;
			float                    progress = (float)i / (float)(points.size() - 1);

			Vector3 tangent;
			if (i < points.size() - 1) {
				tangent = (Vector3(points[i + 1].first.x, points[i + 1].first.y, points[i + 1].first.z) - point)
							  .Normalized();
			} else {
				tangent = (point - Vector3(points[i - 1].first.x, points[i - 1].first.y, points[i - 1].first.z))
							  .Normalized();
			}

			// Parallel transport frame
			Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
			if (normal.MagnitudeSquared() < 1e-6) {
				if (abs(tangent.y) < 0.999) {
					normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				} else {
					normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				}
			} else {
				normal.Normalize();
			}
			Vector3 bitangent = tangent.Cross(normal).Normalized();
			last_normal = normal;

			for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
				float   angle = 2.0f * std::numbers::pi * j / CYLINDER_SEGMENTS;
				Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
				Vector3 pos = point + cn * RADIUS;
				ring.push_back(
					{glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), v_color, progress}
				);
			}
			rings.push_back(ring);
		}

		for (size_t i = 0; i < rings.size() - 1; ++i) {
			for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
				vertices_data.push_back(rings[i][j]);
				vertices_data.push_back(rings[i + 1][j]);
			}
		}

		vertex_count = vertices_data.size();
		if (vertex_count == 0)
			return;

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertices_data.size() * sizeof(TrailVertex), vertices_data.data());
	}

	void Trail::Render(Shader& shader) const {
		if (vertex_count < 2) {
			return;
		}

		shader.use();
		shader.setInt("useVertexColor", 1);

		glm::mat4 model = glm::mat4(1.0f);
		shader.setMat4("model", model);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, vertex_count);
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);
	}

} // namespace Boidsish
