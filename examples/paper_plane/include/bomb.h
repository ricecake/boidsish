#pragma once

// #include <iostream>
// #include <memory>
// #include <random>
// #include <set>
// #include <vector>

// #include "dot.h"
// #include "emplacements.h"
// #include "field.h"
// #include "graphics.h"
// #include "guided_missile.h"
// #include "handler.h"
// #include "hud.h"
// #include "logger.h"
#include "model.h"
#include "fire_effect.h"
#include "entity.h"
// #include "plane.h"
// #include "spatial_entity_handler.h"
// #include "terrain_generator.h"
// #include <GLFW/glfw3.h>
#include <fire_effect.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

class CatBomb: public Entity<Model> {
public:
	CatBomb(int id = 0, Vector3 pos = {0, 0, 0}, glm::vec3 dir = {0, 0, 0}, Vector3 vel = {0, 0, 0}):
		Entity<Model>(id, "assets/bomb_shading_v005.obj", true) {
		SetOrientToVelocity(true);
		SetPosition(pos.x, pos.y, pos.z);
		auto netVelocity = glm::vec3(vel.x, vel.y, vel.z) + 2.5f * glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
		SetVelocity(netVelocity.x, netVelocity.y, netVelocity.z);

		SetTrailLength(50);
		shape_->SetScale(glm::vec3(0.01f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
		);
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		auto pos = GetPosition();
		lived_ += delta_time;

		if (exploded_) {
			// If exploded, just wait to be removed
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		// --- Ground/Terrain Collision ---
		auto [height, norm] = handler.vis->GetTerrainPointProperties(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler);
			return;
		}

		// --- Gravity ---
		auto velo = GetVelocity();
		velo += Vector3(0, -kGravity, 0);
		SetVelocity(velo);
	}

private:
	void Explode(const EntityHandler& handler) {
		if (exploded_)
			return;

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0), // direction
				glm::vec3(0, 0, 0), // velocity
				-1,                 // max_particles
				2.0f                // lifetime
			);
		});

		exploded_ = true;
		lived_ = 0.0f; // Reset lived timer for explosion phase
		SetVelocity(Vector3(0, 0, 0));
		SetTrailLength(0); // Stop emitting trail
	}

	// Constants
	static constexpr float kGravity = 0.15f;
	static constexpr float kExplosionDisplayTime = 2.0f; // How long the bomb object sticks around after exploding

	// State
	float lived_ = 0.0f;
	bool  exploded_ = false;
};
