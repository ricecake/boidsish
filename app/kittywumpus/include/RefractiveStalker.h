#pragma once

#include "entity.h"
#include "walking_creature.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class RefractiveStalker : public Entity<WalkingCreature> {
	public:
		RefractiveStalker(int id, const glm::vec3& pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage) override;

		bool IsTargetable() const override { return health_ > 0; }

	private:
		enum class State {
			STALKING,
			ATTACKING,
			COOLDOWN,
			DYING
		};

		State state_ = State::STALKING;
		float health_ = 50.0f;
		float state_timer_ = 0.0f;
		int beam_id_ = -1;

		float base_height_ = 0.0f;
		float stalk_speed_ = 15.0f;
		float attack_dist_ = 60.0f;

		void UpdateStalking(const EntityHandler& handler, float delta_time);
		void UpdateAttacking(const EntityHandler& handler, float delta_time);
		void UpdateCooldown(const EntityHandler& handler, float delta_time);
	};

} // namespace Boidsish
