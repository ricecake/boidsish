#pragma once

#include <random>

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class FighterPlane: public Entity<Model> {
	public:
		enum class State { CIRCLING, ENGAGING, CRASHING };

		FighterPlane(int id, int launcher_id, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void ShotDown(const EntityHandler& handler);
		void Explode(const EntityHandler& handler);

		State GetState() const { return state_; }
		int   GetLauncherId() const { return launcher_id_; }

	private:
		int   launcher_id_;
		State state_ = State::CIRCLING;
		float lived_ = 0.0f;
		float fire_timer_ = 0.0f;
		float spiral_timer_ = 0.0f;
		bool  exploded_ = false;

		std::random_device rd_;
		std::mt19937       eng_;

		// Behavior constants
		static constexpr float kCirclingSpeed = 30.0f;
		static constexpr float kEngagingSpeed = 60.0f;
		static constexpr float kEngagementRadius = 400.0f;
		static constexpr float kDisengagementRadius = 600.0f;
		static constexpr float kCirclingRadius = 150.0f;
		static constexpr float kFireInterval = 0.75f;
	};

} // namespace Boidsish
