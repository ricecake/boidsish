#include "hot_air_balloon.h"
#include "weather_manager.h"
#include <cmath>
#include <algorithm>

namespace Boidsish {

	HotAirBalloonEntity::HotAirBalloonEntity(
		int id,
		float x,
		float y,
		float z,
		float size,
		const glm::vec4& color,
		float buoyancy,
		float wind_response
	) :
		EntityBase(id),
		name_("Balloon #" + std::to_string(id)),
		buoyancy_(buoyancy),
		wind_response_(wind_response),
		color_(color)
	{
		size_ = size;
		SetPosition(x, y, z);
		InitializePhysics();
	}

	HotAirBalloonEntity::~HotAirBalloonEntity() {
		// Shape is removed from visualizer in EntityHandler or automatically on deletion
	}

	void HotAirBalloonEntity::InitializePhysics() {
		points_.clear();
		springs_.clear();

		shape_ = std::make_shared<DelaunayBlob>(id_);
		shape_->SetRenderMode(DelaunayBlob::RenderMode::SolidWithWire);
		shape_->SetSmoothNormals(true);

		// Initial position of balloon center
		glm::vec3 center(GetXPos(), GetYPos(), GetZPos());

		const int N_lat = 6;
		const int N_lon = 8;
		float r_max = size_;
		float pi = 3.1415926535f;

		// Helper to add a physical point
		auto add_point = [&](const glm::vec3& rel_pos, bool is_basket) {
			SoftPoint pt;
			pt.rest_offset = rel_pos;
			pt.position = center + rel_pos;
			pt.velocity = glm::vec3(0.0f);
			pt.force = glm::vec3(0.0f);
			pt.mass = is_basket ? 1.5f : 1.0f; // Basket is heavier
			pt.is_basket = is_basket;

			if (is_basket) {
				pt.color = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f); // Brown basket
			} else {
				// Stripe pattern based on longitude slice
				float angle = atan2(rel_pos.z, rel_pos.x);
				if (angle < 0.0f) angle += 2.0f * pi;
				int stripe_idx = static_cast<int>(angle / (2.0f * pi / N_lon));
				if (stripe_idx % 2 == 0) {
					pt.color = color_;
				} else {
					pt.color = glm::vec4(1.0f, 0.9f, 0.2f, color_.a); // Yellow stripes
				}
			}

			pt.id = shape_->AddPoint(pt.position);
			shape_->SetPointColor(pt.id, pt.color);
			points_.push_back(pt);
			return static_cast<int>(points_.size() - 1);
		};

		// Add top pole
		int top_pole_idx = add_point(glm::vec3(0.0f, r_max, 0.0f), false);

		// Add envelope slices
		std::vector<std::vector<int>> slice_indices(N_lat - 1);
		for (int i = 1; i < N_lat; ++i) {
			float lat_frac = static_cast<float>(i) / N_lat;
			float y_rel = r_max * (1.0f - 2.0f * lat_frac);

			// Compute radius at y_rel
			float r_rel = 0.0f;
			float y_norm = y_rel / r_max; // -1 to 1
			if (y_norm > 0.0f) {
				r_rel = r_max * sqrt(1.0f - y_norm * y_norm);
			} else {
				float f = (y_norm + 1.0f); // 0 at neck, 1 at equator
				r_rel = r_max * (0.3f + 0.7f * (1.5f * f - 0.5f * f * f * f));
			}

			for (int j = 0; j < N_lon; ++j) {
				float theta = j * 2.0f * pi / N_lon;
				glm::vec3 rel_pos(r_rel * cos(theta), y_rel, r_rel * sin(theta));
				int idx = add_point(rel_pos, false);
				slice_indices[i - 1].push_back(idx);
			}
		}

		// Add bottom pole
		int bottom_pole_idx = add_point(glm::vec3(0.0f, -r_max * 1.1f, 0.0f), false);

		// Add basket points (4 points forming a square suspended below the balloon)
		float basket_y = -r_max * 1.5f;
		float basket_w = r_max * 0.25f;
		int b1 = add_point(glm::vec3(-basket_w, basket_y, -basket_w), true);
		int b2 = add_point(glm::vec3(basket_w, basket_y, -basket_w), true);
		int b3 = add_point(glm::vec3(basket_w, basket_y, basket_w), true);
		int b4 = add_point(glm::vec3(-basket_w, basket_y, basket_w), true);

		// Helper to add a spring
		auto add_spring = [&](int idx_a, int idx_b, float stiffness) {
			SoftSpring sp;
			sp.index_a = idx_a;
			sp.index_b = idx_b;
			sp.rest_length = glm::distance(points_[idx_a].rest_offset, points_[idx_b].rest_offset);
			sp.stiffness = stiffness;
			springs_.push_back(sp);
		};

		// Connect top pole to top slice
		for (int idx : slice_indices[0]) {
			add_spring(top_pole_idx, idx, spring_stiffness_);
		}

		// Connect slices
		for (int i = 0; i < N_lat - 1; ++i) {
			// Latitude springs (horizontal rings)
			for (int j = 0; j < N_lon; ++j) {
				int current = slice_indices[i][j];
				int next = slice_indices[i][(j + 1) % N_lon];
				add_spring(current, next, spring_stiffness_ * 1.2f);
			}

			// Longitude springs (vertical connections)
			if (i < N_lat - 2) {
				for (int j = 0; j < N_lon; ++j) {
					add_spring(slice_indices[i][j], slice_indices[i+1][j], spring_stiffness_);
				}
			}
		}

		// Connect bottom pole to bottom slice
		for (int idx : slice_indices[N_lat - 2]) {
			add_spring(bottom_pole_idx, idx, spring_stiffness_);
		}

		// Radial structural springs (to maintain volume)
		for (int i = 0; i < N_lat - 1; ++i) {
			for (int j = 0; j < N_lon; ++j) {
				int opp_j = (j + N_lon / 2) % N_lon;
				add_spring(slice_indices[i][j], slice_indices[i][opp_j], spring_stiffness_ * 0.5f);
			}
		}

		// Connect basket points to bottom slice (neck) for suspension
		int neck_num = static_cast<int>(slice_indices[N_lat - 2].size());
		for (int j = 0; j < neck_num; ++j) {
			int neck_idx = slice_indices[N_lat - 2][j];
			add_spring(b1, neck_idx, spring_stiffness_ * 0.8f);
			add_spring(b2, neck_idx, spring_stiffness_ * 0.8f);
			add_spring(b3, neck_idx, spring_stiffness_ * 0.8f);
			add_spring(b4, neck_idx, spring_stiffness_ * 0.8f);
		}

		// Basket structural springs
		add_spring(b1, b2, spring_stiffness_ * 2.0f);
		add_spring(b2, b3, spring_stiffness_ * 2.0f);
		add_spring(b3, b4, spring_stiffness_ * 2.0f);
		add_spring(b4, b1, spring_stiffness_ * 2.0f);
		add_spring(b1, b3, spring_stiffness_ * 2.0f); // diagonal
		add_spring(b2, b4, spring_stiffness_ * 2.0f); // diagonal

		shape_->Retetrahedralize();
	}

	void HotAirBalloonEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (delta_time <= 0.0f)
			return;

		float dt = std::min(delta_time, 0.05f);

		// Get WeatherManager
		WeatherManager* weather_mgr = handler.vis ? handler.vis->GetWeatherManager() : nullptr;

		// 1. Reset forces
		for (auto& pt : points_) {
			pt.force = glm::vec3(0.0f);
		}

		// Compute centroid of current positions
		glm::vec3 old_centroid(0.0f);
		for (const auto& pt : points_) {
			old_centroid += pt.position;
		}
		old_centroid /= static_cast<float>(points_.size());

		// 2. Accumulate external forces
		for (auto& pt : points_) {
			// Gravity
			pt.force += glm::vec3(0.0f, -9.81f, 0.0f) * pt.mass;

			// Buoyancy / Lift (only on non-basket points)
			if (!pt.is_basket) {
				pt.force += glm::vec3(0.0f, buoyancy_, 0.0f);
			}

			// Wind response
			if (weather_mgr) {
				PhysicallyBasedWeatherOutput weather_out = weather_mgr->GetWeatherAtPosition(pt.position);
				glm::vec3 wind_vel(weather_out.windVelocity.x, weather_out.verticalWind, weather_out.windVelocity.y);
				pt.force += wind_response_ * (wind_vel - pt.velocity) * 1.5f;
			}

			// Subtle organic bobbing
			if (!pt.is_basket) {
				pt.force.y += sin(time * 1.5f + pt.position.x * 0.1f) * 1.0f;
			}
		}

		// 3. Accumulate spring forces
		for (const auto& sp : springs_) {
			auto& pt_a = points_[sp.index_a];
			auto& pt_b = points_[sp.index_b];

			glm::vec3 diff = pt_b.position - pt_a.position;
			float dist = glm::length(diff);
			if (dist > 0.0001f) {
				glm::vec3 dir = diff / dist;
				float force_magnitude = sp.stiffness * (dist - sp.rest_length);

				// Damping force
				glm::vec3 rel_vel = pt_b.velocity - pt_a.velocity;
				float damping = glm::dot(rel_vel, dir) * spring_damping_;

				glm::vec3 total_force = (force_magnitude + damping) * dir;
				pt_a.force += total_force;
				pt_b.force -= total_force;
			}
		}

		// 4. Shape restoration (pull points back to their local rest offsets to prevent collapse)
		for (auto& pt : points_) {
			glm::vec3 ideal_pos = old_centroid + pt.rest_offset;
			glm::vec3 restore_force = (ideal_pos - pt.position) * (spring_stiffness_ * 0.5f);
			pt.force += restore_force;
		}

		// 5. Integrate and apply terrain collision
		for (auto& pt : points_) {
			// Acceleration
			glm::vec3 acc = pt.force / pt.mass;
			pt.velocity += acc * dt;

			// Air damping
			pt.velocity *= (1.0f - 0.5f * dt);

			// Update position
			pt.position += pt.velocity * dt;

			// Terrain collision
			float terrain_h = 0.0f;
			if (handler.vis) {
				auto [h, norm] = handler.GetTerrainPropertiesAtPoint(pt.position.x, pt.position.z);
				terrain_h = h;
			}

			float ground_y = std::max(0.0f, terrain_h);
			if (pt.position.y < ground_y + 0.1f) {
				pt.position.y = ground_y + 0.1f;
				if (pt.velocity.y < 0.0f) {
					pt.velocity.y = -pt.velocity.y * 0.1f; // low bounce
				}
				pt.velocity.x *= 0.7f;
				pt.velocity.z *= 0.7f;
			}

			// Update shape's point
			shape_->SetPointState(pt.id, pt.position, pt.velocity);
		}

		// Compute new centroid of the balloon
		glm::vec3 new_centroid(0.0f);
		for (const auto& pt : points_) {
			new_centroid += pt.position;
		}
		new_centroid /= static_cast<float>(points_.size());

		// Update EntityBase position
		SetPosition(new_centroid.x, new_centroid.y, new_centroid.z);

		// Retetrahedralize
		shape_->Retetrahedralize();
	}

	std::shared_ptr<Shape> HotAirBalloonEntity::GetShape() const {
		return shape_;
	}

	void HotAirBalloonEntity::UpdateShape() {
		if (shape_) {
			shape_->SetId(id_);
			// We update points in UpdateEntity, so we just make sure color and other parameters are correct
			shape_->SetColor(color_.r, color_.g, color_.b, color_.a);
			shape_->SetRoughness(roughness_);
			shape_->SetMetallic(metallic_);
			shape_->SetUsePBR(use_pbr_);
		}
	}

} // namespace Boidsish
