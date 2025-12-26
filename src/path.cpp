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
#include <glm/gtx/compatibility.hpp>

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
		int              current_segment_index,
		float            current_t,
		int              current_direction,
		float            path_speed,
		float            delta_time
	) const {
		if (waypoints_.size() < 2) {
			return {Vector3(0, 0, 0), glm::quat(), 1, 0, 0.0f};
		}

		int num_waypoints = waypoints_.size();
		int num_segments = (mode_ == PathMode::LOOP) ? num_waypoints : num_waypoints - 1;
		float arrival_radius_sq = 0.05f * 0.05f;
		float distance_to_travel = path_speed * delta_time;

		int   new_segment_index = current_segment_index;
		float new_t = current_t;
		int   new_direction = current_direction;

		// Iteratively move along the path until the frame's travel distance is used up
		while (distance_to_travel > 1e-6) {
			Vector3 p0, p1, p2, p3;
			if (mode_ == PathMode::LOOP) {
				p0 = waypoints_[(new_segment_index - 1 + num_waypoints) % num_waypoints].position;
				p1 = waypoints_[new_segment_index].position;
				p2 = waypoints_[(new_segment_index + 1) % num_waypoints].position;
				p3 = waypoints_[(new_segment_index + 2) % num_waypoints].position;
			} else {
				p1 = waypoints_[new_segment_index].position;
				p2 = waypoints_[new_segment_index + 1].position;
				if (new_segment_index > 0) {
					p0 = waypoints_[new_segment_index - 1].position;
				} else {
					p0 = p1 - (p2 - p1);
				}
				if (new_segment_index < num_segments - 1) {
					p3 = waypoints_[new_segment_index + 2].position;
				} else {
					p3 = p2 + (p2 - p1);
				}
			}

			// Estimate segment length
			float   segment_length = 0;
			Vector3 prev_point = Spline::CatmullRom(0, p0, p1, p2, p3);
			for (int i = 1; i <= 10; ++i) {
				float   t = (float)i / 10.0f;
				Vector3 curr_point = Spline::CatmullRom(t, p0, p1, p2, p3);
				segment_length += (curr_point - prev_point).Magnitude();
				prev_point = curr_point;
			}
			if (segment_length < 1e-6) {
				break;
			}

			float distance_remaining_on_segment = (new_direction > 0) ? (1.0f - new_t) * segment_length
																		: new_t * segment_length;

			if (distance_to_travel <= distance_remaining_on_segment) {
				float t_advance = distance_to_travel / segment_length;
				new_t += t_advance * new_direction;
				distance_to_travel = 0.0f;
			} else {
				distance_to_travel -= distance_remaining_on_segment;
				new_segment_index += new_direction;

				if (new_segment_index >= num_segments) {
					if (mode_ == PathMode::LOOP) {
						new_segment_index = 0;
					} else if (mode_ == PathMode::REVERSE) {
						new_direction = -1;
						new_segment_index = num_segments - 1;
						new_t = 1.0f;
					} else { // ONCE
						new_segment_index = num_segments - 1;
						new_t = 1.0f;
						distance_to_travel = 0;
					}
				} else if (new_segment_index < 0) {
					if (mode_ == PathMode::LOOP) {
						new_segment_index = num_segments - 1;
					} else if (mode_ == PathMode::REVERSE) {
						new_direction = 1;
						new_segment_index = 0;
						new_t = 0.0f;
					} else { // ONCE
						new_segment_index = 0;
						new_t = 0.0f;
						distance_to_travel = 0;
					}
				}
				new_t = (new_direction > 0) ? 0.0f : 1.0f;
			}
		}

		Vector3 p0, p1, p2, p3;
		const Waypoint *next_w1, *next_w2;

		if (mode_ == PathMode::LOOP) {
			p0 = waypoints_[(new_segment_index - 1 + num_waypoints) % num_waypoints].position;
			p1 = waypoints_[new_segment_index].position;
			p2 = waypoints_[(new_segment_index + 1) % num_waypoints].position;
			p3 = waypoints_[(new_segment_index + 2) % num_waypoints].position;
			next_w1 = &waypoints_[new_segment_index];
			next_w2 = &waypoints_[(new_segment_index + 1) % num_waypoints];
		} else {
			p1 = waypoints_[new_segment_index].position;
			p2 = waypoints_[new_segment_index + 1].position;
			if (new_segment_index > 0) {
				p0 = waypoints_[new_segment_index - 1].position;
			} else {
				p0 = p1 - (p2 - p1);
			}
			if (new_segment_index < num_segments - 1) {
				p3 = waypoints_[new_segment_index + 2].position;
			} else {
				p3 = p2 + (p2 - p1);
			}
			next_w1 = &waypoints_[new_segment_index];
			next_w2 = &waypoints_[new_segment_index + 1];
		}

		Vector3 target_position = Spline::CatmullRom(new_t, p0, p1, p2, p3);
		Vector3 desired_velocity = (target_position - current_position).Normalized();

		if (mode_ == PathMode::ONCE && new_segment_index == num_segments - 1 && new_t >= 1.0f) {
			if ((waypoints_.back().position - current_position).MagnitudeSquared() < arrival_radius_sq) {
				desired_velocity.Set(0, 0, 0);
			} else {
				desired_velocity = (waypoints_.back().position - current_position).Normalized();
			}
		}

		Vector3 tangent = Spline::CatmullRomDerivative(new_t, p0, p1, p2, p3).Normalized();
		Vector3 up = next_w1->up * (1.0f - new_t) + next_w2->up * new_t;
		Vector3 right = tangent.Cross(up);
		if (right.MagnitudeSquared() < 1e-6) {
			right = tangent.Cross(Vector3(0, 1, 0)).Normalized();
			if (right.MagnitudeSquared() < 1e-6) {
				right = Vector3(1, 0, 0);
			}
		}
		right.Normalize();
		up = right.Cross(tangent).Normalized();

		glm::mat4 rotationMatrix = glm::lookAt(
			glm::vec3(0, 0, 0),
			glm::vec3(tangent.x, tangent.y, tangent.z),
			glm::vec3(up.x, up.y, up.z)
		);
		glm::quat desired_orientation = glm::conjugate(glm::quat_cast(rotationMatrix));

		return {
			desired_velocity,
			desired_orientation,
			new_direction,
			new_segment_index,
			new_t,
		};
	}

} // namespace Boidsish
