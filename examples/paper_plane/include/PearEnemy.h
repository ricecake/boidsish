#pragma once

#include <random>

#include "PearShape.h"
#include "entity.h"

namespace Boidsish {

	class PearEnemy: public Entity<PearShape> {
	public:
		PearEnemy(int id, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		void Roam(const EntityHandler& handler, float delta_time);

		glm::vec3 target_pos_;
		bool      has_target_ = false;
		float     move_speed_ = 5.0f;
		float     wait_timer_ = 0.0f;
		float     detection_radius_ = 300.0f;
		float     attack_cooldown_ = 0.0f;

		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
