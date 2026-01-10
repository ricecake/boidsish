#pragma once

#include <memory>
#include <random>

#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	// Forward declaration
	class FireEffect;

	class CatMissile: public Entity<Model> {
	public:
		CatMissile(
			int       id = 0,
			Vector3   pos = {0, 0, 0},
			glm::quat orientation = {0, {0, 0, 0}},
			glm::vec3 dir = {0, 0, 0},
			Vector3   vel = {0, 0, 0}
		);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;
		void Explode(const EntityHandler& handler, bool hit_target);

	private:
		// Constants
		static constexpr float lifetime_ = 12.0f;
		static constexpr float kExplosionDisplayTime = 2.0f;
		// State
		float                       lived_ = 0.0f;
		bool                        exploded_ = false;
		bool                        fired_ = false;
		std::shared_ptr<FireEffect> exhaust_effect_ = nullptr;

		// Flight model
		glm::quat          orientation_;
		glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
		float              forward_speed_;
		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
