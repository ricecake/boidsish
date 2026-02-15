#pragma once

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class Potshot: public Entity<Model> {
	public:
		Potshot(int id, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage) override;

	private:
		float     health_ = 30.0f;
		float     lifetime_ = 8.0f;
		float     lived_ = 0.0f;
		float     reposition_timer_ = 0.0f;
		glm::vec3 relative_target_pos_;
		int       shots_to_fire_ = 0;
		float     fire_timer_ = 0.0f;
		float     speed_ = 120.0f;
		bool      initialized_target_ = false;

		void PickNewPosition(const glm::vec3& player_forward);
	};

} // namespace Boidsish
