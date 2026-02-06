#pragma once

#include <memory>
#include <random>

#include "GuidedMissileLauncher.h"
#include "entity.h"
#include "model.h"
#include "sound_effect.h"
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
			Vector3   vel = {0, 0, 0},
			bool      leftHanded = true
		);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void Explode(const EntityHandler& handler, bool hit_target);

	private:
		// Constants
		static constexpr float lifetime_ = 12.0f;
		static constexpr float kExplosionDisplayTime = 2.0f;
		// State
		float                                  lived_ = 0.0f;
		bool                                   exploded_ = false;
		bool                                   fired_ = false;
		std::shared_ptr<FireEffect>            exhaust_effect_ = nullptr;
		std::shared_ptr<EntityBase>            target_ = nullptr;
		std::shared_ptr<SoundEffect>           launch_sound_ = nullptr;
		std::shared_ptr<SoundEffect>           explode_sound_ = nullptr;
		bool                                   leftHanded_ = true;

		// Flight model
		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
