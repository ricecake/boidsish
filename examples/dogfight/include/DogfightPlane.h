#pragma once

#include "dot.h"
#include "entity.h"
#include <random>

namespace Boidsish {

	enum class Team { RED, BLUE };

	class DogfightPlane: public Entity<Dot> {
	public:
		DogfightPlane(int id, Team team, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void Explode(const EntityHandler& handler);

		Team GetTeam() const { return team_; }

	private:
		Team  team_;
		float being_chased_timer_ = 0.0f;
		float fire_timer_ = 0.0f;
		bool  exploded_ = false;
		float lived_ = 0.0f;

		// Evasive maneuver state
		float maneuver_time_ = 0.0f;

		std::random_device rd_;
		std::mt19937       eng_;

		// Behavior constants
		static constexpr float kSlowSpeed = 40.0f;
		static constexpr float kFastSpeed = 100.0f;
		static constexpr float kDetectionRadius = 300.0f;
		static constexpr float kChaseDistance = 30.0f;
		static constexpr float kKillDistance = 60.0f;
		static constexpr float kKillAngle = 0.95f;      // cos(theta)
		static constexpr float kKillBehindAngle = 0.8f; // cos(theta) for victim's rear
		static constexpr float kKillTimeThreshold = 3.0f;
		static constexpr float kBeingChasedThreshold = 5.0f;
	};

} // namespace Boidsish
