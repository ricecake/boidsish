#pragma once

#include <random>

#include "dot.h"
#include "entity.h"
#include "model.h"

namespace Boidsish {

	enum class Team { RED, BLUE };

	class DogfightPlane: public Entity<Model> {
	public:
		DogfightPlane(Team team, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void Explode(const EntityHandler& handler);

		Team GetTeam() const { return team_; }

		std::shared_ptr<DogfightPlane> GetTarget() const { return target_; }

		std::shared_ptr<DogfightPlane> GetChaser() const { return chaser_; }

	private:
		Team                           team_;
		std::shared_ptr<DogfightPlane> target_ = nullptr;
		std::shared_ptr<DogfightPlane> chaser_ = nullptr;
		float                          being_chased_timer_ = 0.0f;
		float                          fire_timer_ = 0.0f;
		bool                           exploded_ = false;
		float                          lived_ = 0.0f;

		// Evasive maneuver state
		float maneuver_time_ = 0.0f;

		std::random_device rd_;
		std::mt19937       eng_;

		// Behavior constants
		static constexpr float kSlowSpeed = 20.0f;
		static constexpr float kFastSpeed = 30.0f;
		static constexpr float kDetectionRadius = 100.0f;
		static constexpr float kChaseDistance = 30.0f;
		static constexpr float kKillDistance = 60.0f;
		static constexpr float kKillAngle = 0.95f;      // cos(theta)
		static constexpr float kKillBehindAngle = 0.8f; // cos(theta) for victim's rear
		static constexpr float kKillTimeThreshold = 3.0f;
		static constexpr float kBeingChasedThreshold = 5.0f;
	};

} // namespace Boidsish
