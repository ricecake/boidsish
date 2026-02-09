
#pragma once

#include <random>

#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class GuidedMissileLauncher: public Entity<Model> {
	public:
		GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void Destroy(const EntityHandler& handler);

		glm::vec3 GetApproachPoint() const { return approach_point_; }

	private:
		glm::vec3                   approach_point_;
		bool                        approach_point_set_ = false;
		float                       time_since_last_fire_ = 0.0f;
		float                       fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized
		static constexpr int        kMaxInFlightMissiles = 5;
		std::shared_ptr<ArcadeText> text_ = nullptr;

		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
