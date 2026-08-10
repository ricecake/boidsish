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
		// Cleanly remove additional shapes from visualizer
		if (basket_added_ && shape_handler_visualizer_pointer_) {
			shape_handler_visualizer_pointer_->RemoveShape(basket_shape_->GetId());
			for (auto& line : line_shapes_) {
				shape_handler_visualizer_pointer_->RemoveShape(line->GetId());
			}
		}
	}

	void HotAirBalloonEntity::InitializePhysics() {
		// If we are re-initializing, remove old extra shapes first
		if (basket_added_ && shape_handler_visualizer_pointer_) {
			shape_handler_visualizer_pointer_->RemoveShape(basket_shape_->GetId());
			for (auto& line : line_shapes_) {
				shape_handler_visualizer_pointer_->RemoveShape(line->GetId());
			}
			basket_added_ = false;
		}

		points_.clear();
		springs_.clear();
		line_shapes_.clear();

		// 1. Envelope Shape (DelaunayBlob)
		shape_ = std::make_shared<DelaunayBlob>(id_);
		shape_->SetRenderMode(DelaunayBlob::RenderMode::SolidWithWire);
		shape_->SetSmoothNormals(true);

		// 2. Basket Shape (Polyhedron Cube)
		basket_shape_ = std::make_shared<Polyhedron>(
			PolyhedronType::Cube,
			Shape::s_nextId++,
			GetXPos(), GetYPos() - size_ * 1.5f, GetZPos(),
			1.0f, // size multiplier
			0.4f, 0.25f, 0.15f, 1.0f // Brown basket
		);
		basket_shape_->SetScale(glm::vec3(size_ * 0.2f, size_ * 0.15f, size_ * 0.2f));

		// 3. Line Shapes (4 suspension cables)
		for (int i = 0; i < 4; ++i) {
			auto line = std::make_shared<Line>(
				Shape::s_nextId++,
				glm::vec3(0.0f),
				glm::vec3(0.0f),
				size_ * 0.04f, // thickness
				0.7f, 0.7f, 0.7f, 1.0f // Silver/grey lines
			);
			line_shapes_.push_back(line);
		}

		// Initial position of balloon center
		glm::vec3 center(GetXPos(), GetYPos(), GetZPos());

		const int num_envelope_points = 32;
		float r_max = size_;
		float pi = 3.1415926535f;

		// Helper to add a physical point
		auto add_point = [&](const glm::vec3& rel_pos, bool is_basket) {
			SoftPoint pt;
			pt.rest_offset = rel_pos;
			pt.position = center + rel_pos;
			pt.velocity = glm::vec3(0.0f);
			pt.force = glm::vec3(0.0f);
			pt.mass = is_basket ? 2.5f : 1.0f; // Basket is heavy
			pt.is_basket = is_basket;

			if (is_basket) {
				pt.color = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f);
			} else {
				// Stripe pattern based on angle
				float angle = atan2(rel_pos.z, rel_pos.x);
				if (angle < 0.0f) angle += 2.0f * pi;
				int stripe_idx = static_cast<int>(angle / (2.0f * pi / 8.0f));
				if (stripe_idx % 2 == 0) {
					pt.color = color_;
				} else {
					pt.color = glm::vec4(1.0f, 0.9f, 0.2f, color_.a); // Yellow stripes
				}
			}

			if (!is_basket) {
				pt.id = shape_->AddPoint(pt.position);
				shape_->SetPointColor(pt.id, pt.color);
			} else {
				pt.id = -1;
			}
			points_.push_back(pt);
			return static_cast<int>(points_.size() - 1);
		};

		// Generate envelope points on a beautiful, convex stretched sphere
		for (int i = 0; i < num_envelope_points; ++i) {
			float y_norm = 1.0f - (i / (float)(num_envelope_points - 1)) * 2.0f; // -1 to 1
			float y_rel = y_norm * r_max * 1.2f;

			// Taper the bottom part slightly to resemble a teardrop balloon envelope
			float r_factor = 1.0f;
			if (y_norm < 0.0f) {
				r_factor = 1.0f + 0.3f * y_norm; // tapers smoothly down to 0.7 at the bottom pole
			}
			float radius_at_y = r_factor * r_max * sqrt(1.0f - y_norm * y_norm);

			float golden_ratio = 1.6180339887f;
			float theta = 2.0f * pi * i / golden_ratio;

			glm::vec3 rel_pos(
				radius_at_y * cos(theta),
				y_rel,
				radius_at_y * sin(theta)
			);
			add_point(rel_pos, false);
		}

		// Generate 1 basket point at the bottom
		int basket_idx = add_point(glm::vec3(0.0f, -r_max * 1.6f, 0.0f), true);

		// Helper to add a spring
		auto add_spring = [&](int idx_a, int idx_b, float stiffness) {
			SoftSpring sp;
			sp.index_a = idx_a;
			sp.index_b = idx_b;
			sp.rest_length = glm::distance(points_[idx_a].rest_offset, points_[idx_b].rest_offset);
			sp.stiffness = stiffness;
			springs_.push_back(sp);
		};

		// Structural lattice springs: connect each envelope point to its nearest 5 neighbors
		for (int i = 0; i < num_envelope_points; ++i) {
			std::vector<std::pair<float, int>> dists;
			for (int j = 0; j < num_envelope_points; ++j) {
				if (i == j) continue;
				float d = glm::distance(points_[i].rest_offset, points_[j].rest_offset);
				dists.push_back({d, j});
			}
			std::sort(dists.begin(), dists.end());
			for (size_t k = 0; k < std::min<size_t>(5, dists.size()); ++k) {
				add_spring(i, dists[k].second, spring_stiffness_ * 1.5f);
			}
		}

		// Connect basket point to the 6 lowest envelope points for physical suspension
		std::vector<std::pair<float, int>> lowest_pts;
		for (int i = 0; i < num_envelope_points; ++i) {
			lowest_pts.push_back({points_[i].rest_offset.y, i});
		}
		std::sort(lowest_pts.begin(), lowest_pts.end()); // sorts ascending (lowest y first)

		for (size_t k = 0; k < std::min<size_t>(6, lowest_pts.size()); ++k) {
			add_spring(basket_idx, lowest_pts[k].second, spring_stiffness_ * 1.5f);
		}

		shape_->Retetrahedralize();
	}

	void HotAirBalloonEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (delta_time <= 0.0f)
			return;

		float dt = std::min(delta_time, 0.05f);

		// Store visualizer pointer for clean deletion later
		if (handler.vis) {
			shape_handler_visualizer_pointer_ = handler.vis.get();
		}

		// Add additional shapes to the visualizer if they haven't been added yet
		if (!basket_added_ && handler.vis) {
			handler.vis->AddShape(basket_shape_);
			for (auto& line : line_shapes_) {
				handler.vis->AddShape(line);
			}
			basket_added_ = true;
		}

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

		// Get weather properties
		float wind_str = 1.0f;
		float wind_sp = 1.0f;
		float temp = 288.15f;
		if (weather_mgr) {
			const auto& w = weather_mgr->GetCurrentWeather();
			wind_str = w.wind_strength;
			wind_sp = w.wind_speed;
			temp = w.temperature;
		}

		// Calculate slowly changing rotating global fallback/ambient wind
		float wind_t = time * 0.15f * wind_sp;
		glm::vec3 ambient_wind(
			cos(wind_t) * wind_str * 6.0f,
			0.0f,
			sin(wind_t * 0.7f) * wind_str * 6.0f
		);

		// 2. Accumulate external forces
		for (auto& pt : points_) {
			// Gravity
			pt.force += glm::vec3(0.0f, -9.81f, 0.0f) * pt.mass;

			// Buoyancy / Lift (only on non-basket points)
			if (!pt.is_basket) {
				pt.force += glm::vec3(0.0f, buoyancy_, 0.0f);
			}

			// Localized wind & updrafts (blend LBM with organic fallback)
			glm::vec3 point_wind = ambient_wind;
			if (weather_mgr) {
				PhysicallyBasedWeatherOutput weather_out = weather_mgr->GetWeatherAtPosition(pt.position);
				// If LBM wind is active/valid, use it
				if (glm::length(weather_out.windVelocity) > 0.001f || std::abs(weather_out.verticalWind) > 0.001f) {
					point_wind = glm::vec3(weather_out.windVelocity.x, weather_out.verticalWind, weather_out.windVelocity.y);
				}
			}

			// Thermal updraft columns (highly realistic rising/sinking column zones)
			float terrain_h = 0.0f;
			if (handler.vis) {
				auto [h, norm] = handler.GetTerrainPropertiesAtPoint(pt.position.x, pt.position.z);
				terrain_h = h;
			}

			// Peaks create strong thermal columns; periodic sin/cos waves create thermal column bands
			float peak_updraft = std::max(0.0f, terrain_h - 15.0f) * 0.03f;
			float column_updraft = (sin(pt.position.x * 0.01f) * cos(pt.position.z * 0.01f) + 1.0f) * 0.5f; // 0 to 1 columns
			float local_updraft = (peak_updraft * 2.0f + column_updraft * 3.0f) * (temp / 288.15f);

			point_wind.y += local_updraft;

			// Wind aerodynamic response / drag
			pt.force += wind_response_ * (point_wind - pt.velocity) * 1.5f;

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

		// 5. Integrate and apply terrain/ground collision
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

			// Update shape's point if it belongs to the envelope
			if (!pt.is_basket) {
				shape_->SetPointState(pt.id, pt.position, pt.velocity);
			}
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

		// Update basket shape position
		int basket_idx = static_cast<int>(points_.size() - 1);
		glm::vec3 basket_pos = points_[basket_idx].position;
		basket_shape_->SetPosition(basket_pos.x, basket_pos.y, basket_pos.z);

		// Update line shapes (4 cables hanging down)
		float w = size_ * 0.2f;
		float h = size_ * 0.15f;
		glm::vec3 c1 = basket_pos + glm::vec3(-w, h, -w);
		glm::vec3 c2 = basket_pos + glm::vec3(w, h, -w);
		glm::vec3 c3 = basket_pos + glm::vec3(w, h, w);
		glm::vec3 c4 = basket_pos + glm::vec3(-w, h, w);

		// Find the lowest 4 points on the envelope to attach cables
		std::vector<std::pair<float, int>> lowest_pts;
		for (size_t i = 0; i < points_.size() - 1; ++i) {
			lowest_pts.push_back({points_[i].rest_offset.y, static_cast<int>(i)});
		}
		std::sort(lowest_pts.begin(), lowest_pts.end());

		line_shapes_[0]->SetStart(points_[lowest_pts[0].second].position);
		line_shapes_[0]->SetEnd(c1);

		line_shapes_[1]->SetStart(points_[lowest_pts[1].second].position);
		line_shapes_[1]->SetEnd(c2);

		line_shapes_[2]->SetStart(points_[lowest_pts[2].second].position);
		line_shapes_[2]->SetEnd(c3);

		line_shapes_[3]->SetStart(points_[lowest_pts[3].second].position);
		line_shapes_[3]->SetEnd(c4);
	}

	std::shared_ptr<Shape> HotAirBalloonEntity::GetShape() const {
		return shape_;
	}

	void HotAirBalloonEntity::UpdateShape() {
		if (shape_) {
			shape_->SetId(id_);
			shape_->SetColor(color_.r, color_.g, color_.b, color_.a);
			shape_->SetRoughness(roughness_);
			shape_->SetMetallic(metallic_);
			shape_->SetUsePBR(use_pbr_);
		}
	}

} // namespace Boidsish
