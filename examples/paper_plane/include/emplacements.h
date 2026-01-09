#pragma once

// #include <iostream>
// #include <memory>
// #include <random>
// #include <set>
// #include <vector>

// #include "arrow.h"
// #include "bomb.h"
// #include "dot.h"
// #include "field.h"
// #include "graphics.h"
#include "guided_missile.h"
// #include "handler.h"
// #include "hud.h"
// #include "logger.h"
// #include "model.h"
#include "plane.h"
// #include "spatial_entity_handler.h"
// #include "terrain_generator.h"
// #include <GLFW/glfw3.h>
#include <fire_effect.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

class GuidedMissileLauncher: public Entity<Model> {
public:
	GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation):
		Entity<Model>(id, "assets/utah_teapot.obj", false), eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		shape_->SetScale(glm::vec3(2.0f)); // Set a visible scale
		shape_->SetBaseRotation(glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		shape_->SetRotation(orientation);
		// shape_->SetBaseRotation(orientation);
		std::uniform_real_distribution<float> dist(4.0f, 8.0f);
		fire_interval_ = dist(eng_);

		UpdateShape();
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		time_since_last_fire_ += delta_time;
		if (time_since_last_fire_ < fire_interval_) {
			return;
		}

		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;
		auto plane = planes[0];

		// Check distance to plane
		float distance_to_plane = (plane->GetPosition() - GetPosition()).Magnitude();
		if (distance_to_plane > 500.0f) {
			return;
		}

		auto pos = GetPosition();
		// Calculate firing probability based on altitude
		auto  ppos = plane->GetPosition();
		auto  pvel = plane->GetVelocity();
		float max_h = handler.vis->GetTerrainMaxHeight();

		if (max_h <= 0.0f)
			max_h = 200.0f; // Fallback

		float start_h = 60.0f;
		float extreme_h = 3.0f * max_h;

		if (ppos.y < start_h)
			return;

		const float p_min = 0.5f;
		const float p_max = 10.0f;

		auto directionWeight = glm::dot(glm::normalize(pvel), glm::normalize(pos - ppos));

		float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
		norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f);

		float missiles_per_second = p_min + (p_max - p_min) * norm_alt;
		float fire_probability_this_frame = missiles_per_second * directionWeight * delta_time;

		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		if (dist(eng_) < fire_probability_this_frame) {
			handler.QueueAddEntity<GuidedMissile>(GetPosition());
			time_since_last_fire_ = 0.0f;
			// Set a new random interval for the next shot
			std::uniform_real_distribution<float> new_dist(4.0f, 8.0f);
			fire_interval_ = new_dist(eng_);
		}
	}

	float time_since_last_fire_ = 0.0f;
	float fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized

	std::random_device rd_;
	std::mt19937       eng_;
};
