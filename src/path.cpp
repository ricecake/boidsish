#include "path.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "dot.h"
#include "shader.h"
#include "spline.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	Path::Path(int id, float x, float y, float z): Shape(id, x, y, z, 1.0f, 1.0f, 1.0f, 1.0f, 0) {}

	Path::~Path() {
		if (buffers_initialized_) {
			glDeleteVertexArrays(1, &path_vao_);
			glDeleteBuffers(1, &path_vbo_);
		}
	}

	void Path::SetupBuffers() const {
		if (waypoints_.size() < 2)
			return;

		std::vector<Vector3>   points;
		std::vector<Vector3>   ups;
		std::vector<float>     sizes;
		std::vector<glm::vec3> colors;

		for (const auto& waypoint : waypoints_) {
			points.push_back(waypoint.position);
			ups.push_back(waypoint.up);
			sizes.push_back(waypoint.size);
			colors.push_back(glm::vec3(waypoint.r, waypoint.g, waypoint.b));
		}

		auto all_vertices_data = Spline::GenerateTube(points, ups, sizes, colors, mode_ == PathMode::LOOP);
		edge_vertex_count_ = all_vertices_data.size();

		if (path_vao_ == 0)
			glGenVertexArrays(1, &path_vao_);
		glBindVertexArray(path_vao_);
		if (path_vbo_ == 0)
			glGenBuffers(1, &path_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, path_vbo_);
		glBufferData(
			GL_ARRAY_BUFFER,
			all_vertices_data.size() * sizeof(Spline::VertexData),
			all_vertices_data.data(),
			GL_STATIC_DRAW
		);

		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, pos)
		);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, normal)
		);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			2,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Spline::VertexData),
			(void*)offsetof(Spline::VertexData, color)
		);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		buffers_initialized_ = true;
	}

	void Path::render() const {
		if (!visible_)
			return;

		if (!buffers_initialized_) {
			SetupBuffers();
		}

		for (const auto& waypoint : waypoints_) {
			Dot(0,
			    waypoint.position.x + GetX(),
			    waypoint.position.y + GetY(),
			    waypoint.position.z + GetZ(),
			    waypoint.size,
			    waypoint.r,
			    waypoint.g,
			    waypoint.b,
			    waypoint.a,
			    0)
				.render();
		}

		if (edge_vertex_count_ > 0) {
			shader->use();
			shader->setInt("useVertexColor", 1);

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
			shader->setMat4("model", model);

			glBindVertexArray(path_vao_);
			glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
			glBindVertexArray(0);

			shader->setInt("useVertexColor", 0);
			model = glm::mat4(1.0f);
			shader->setMat4("model", model);
		}
	}

	PathUpdateResult Path::CalculateUpdate(
		const Vector3&   current_position,
		const glm::quat& current_orientation,
		int              current_direction,
		float            delta_time
	) const {
		if (waypoints_.size() < 2) {
			return {Vector3(0, 0, 0), glm::quat(), 1};
		}

		// Find the closest point on the path
		float min_dist_sq = -1.0f;
		int   closest_segment = 0;
		float closest_t = 0.0f;

		for (size_t i = 0; i < waypoints_.size() - 1; ++i) {
			const auto& w1 = waypoints_[i];
			const auto& w2 = waypoints_[i + 1];

			Vector3 p0, p1, p2, p3;
			p1 = w1.position;
			p2 = w2.position;

			if (i > 0) {
				p0 = waypoints_[i - 1].position;
			} else if (mode_ == PathMode::LOOP) {
				p0 = waypoints_.back().position;
			} else {
				p0 = w1.position - (w2.position - w1.position);
			}

			if (i < waypoints_.size() - 2) {
				p3 = waypoints_[i + 2].position;
			} else if (mode_ == PathMode::LOOP) {
				p3 = waypoints_.front().position;
			} else {
				p3 = w2.position + (w2.position - w1.position);
			}

			// Search for the closest point on this segment
			for (int j = 0; j <= 20; ++j) {
				float   t = (float)j / 20.0f;
				Vector3 point_on_spline = Spline::CatmullRom(t, p0, p1, p2, p3);
				float   dist_sq = (point_on_spline - current_position).MagnitudeSquared();
				if (min_dist_sq < 0 || dist_sq < min_dist_sq) {
					min_dist_sq = dist_sq;
					closest_segment = i;
					closest_t = t;
				}
			}
		}

		// Now that we have the closest point, we can calculate the target position
		// a short distance ahead on the path.
		float lookahead_dist = 0.1f * current_direction;
		float lookahead_t = closest_t + lookahead_dist;
		int   lookahead_segment = closest_segment;
		int   new_direction = current_direction;

		if (lookahead_t > 1.0f) {
			lookahead_t -= 1.0f;
			lookahead_segment++;
			if (lookahead_segment >= (int)waypoints_.size() - 1) {
				if (mode_ == PathMode::LOOP) {
					lookahead_segment = 0;
				} else if (mode_ == PathMode::REVERSE) {
					new_direction = -1;
					lookahead_segment = waypoints_.size() - 2;
					lookahead_t = 1.0f;
				} else { // ONCE
					lookahead_segment = waypoints_.size() - 2;
					lookahead_t = 1.0f;
				}
			}
		} else if (lookahead_t < 0.0f) {
			lookahead_t += 1.0f;
			lookahead_segment--;
			if (lookahead_segment < 0) {
				if (mode_ == PathMode::LOOP) {
					lookahead_segment = waypoints_.size() - 2;
				} else if (mode_ == PathMode::REVERSE) {
					new_direction = 1;
					lookahead_segment = 0;
					lookahead_t = 0.0f;
				} else { // ONCE
					lookahead_segment = 0;
					lookahead_t = 0.0f;
				}
			}
		}

		const auto& w1 = waypoints_[lookahead_segment];
		const auto& w2 = waypoints_[lookahead_segment + 1];

		Vector3 p0, p1, p2, p3;
		p1 = w1.position;
		p2 = w2.position;

		if (lookahead_segment > 0) {
			p0 = waypoints_[lookahead_segment - 1].position;
		} else if (mode_ == PathMode::LOOP) {
			p0 = waypoints_.back().position;
		} else {
			p0 = w1.position - (w2.position - w1.position);
		}

		if (lookahead_segment < (int)waypoints_.size() - 2) {
			p3 = waypoints_[lookahead_segment + 2].position;
		} else if (mode_ == PathMode::LOOP) {
			p3 = waypoints_.front().position;
		} else {
			p3 = w2.position + (w2.position - w1.position);
		}

		Vector3 target_position = Spline::CatmullRom(lookahead_t, p0, p1, p2, p3);
		Vector3 desired_velocity = (target_position - current_position).Normalized();

		// Calculate orientation
		Vector3 tangent = (Spline::CatmullRom(lookahead_t + 0.01f * new_direction, p0, p1, p2, p3) - target_position)
							  .Normalized();
		Vector3 up = w1.up * (1.0f - lookahead_t) + w2.up * lookahead_t;
		Vector3 right = tangent.Cross(up);
		if (right.MagnitudeSquared() < 1e-6) {
			right = Vector3(1, 0, 0);
		}
		right.Normalize();
		up = right.Cross(tangent).Normalized();

		glm::mat4 rotationMatrix = glm::lookAt(
			glm::vec3(0, 0, 0),
			glm::vec3(tangent.x, tangent.y, tangent.z),
			glm::vec3(up.x, up.y, up.z)
		);
		glm::quat desired_orientation = glm::conjugate(glm::quat_cast(rotationMatrix));

		return {desired_velocity, desired_orientation, new_direction};
	}

} // namespace Boidsish
