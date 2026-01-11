#pragma once

#include <random>

#include "entity.h"
#include "model.h"
#include "PointDefenseCannon.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class GuidedMissileLauncher: public Entity<Model>, public std::enable_shared_from_this<GuidedMissileLauncher> {
	public:
		GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation);

        void Initialize();
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
        std::shared_ptr<PointDefenseCannon> GetCannon() const;

	private:
		float time_since_last_fire_ = 0.0f;
		float fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized

		std::random_device rd_;
		std::mt19937       eng_;
        std::shared_ptr<PointDefenseCannon> cannon_;
	};

} // namespace Boidsish
