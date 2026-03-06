#pragma once

#include <random>
#include "entity.h"
#include "model.h"

namespace Boidsish {

	class ChargerEnemy : public Entity<Model> {
	public:
		ChargerEnemy(int id, Vector3 pos);

		using EntityBase::OnHit;
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) override;

		bool IsTargetable() const override { return health_ > 0; }

	private:
		enum class State {
			ROAMING,
			POSITIONING,
			PREPARING,
			CHARGING,
			TUMBLING,
			DYING
		};

		State state_ = State::ROAMING;
		float health_ = 50.0f;
		float state_timer_ = 0.0f;
		float dissolve_sweep_ = 1.0f;

		glm::vec3 visual_offset_ = glm::vec3(0.0f);
		glm::vec3 target_pos_ = glm::vec3(0.0f);

		std::random_device rd_;
		std::mt19937       gen_{rd_()};

		void UpdateRoaming(const EntityHandler& handler, float delta_time);
		void UpdatePositioning(const EntityHandler& handler, float delta_time);
		void UpdatePreparing(const EntityHandler& handler, float delta_time);
		void UpdateCharging(const EntityHandler& handler, float delta_time);
		void UpdateTumbling(const EntityHandler& handler, float delta_time);
		void UpdateDying(const EntityHandler& handler, float delta_time);
	};

} // namespace Boidsish
