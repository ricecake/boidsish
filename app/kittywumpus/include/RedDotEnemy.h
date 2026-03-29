#pragma once

#include "entity.h"
#include "dot.h"

namespace Boidsish {

	class RedDotEnemy : public Entity<Dot> {
	public:
		RedDotEnemy(int id, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage) override;

		bool IsTargetable() const override { return state_ == State::ALIVE; }
		bool IsThreat() const override { return state_ == State::ALIVE; }

	private:
		enum class State {
			ALIVE,
			DYING,
			DEAD
		};

		State state_ = State::ALIVE;
		float health_ = 10.0f;
		float death_timer_ = 0.0f;
		static constexpr float kDeathDuration = 3.0f;

		float fire_timer_ = 0.0f;
		float hover_timer_ = 0.0f;
		glm::vec3 hover_offset_;

		void PickHoverOffset();
	};

} // namespace Boidsish
