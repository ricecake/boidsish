
#pragma once

#include <random>

#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class GuidedMissileLauncher: public Entity<Model> {
	public:
		GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation);

		using EntityBase::OnHit;
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) override;
		void Destroy(const EntityHandler& handler, const glm::vec3& hit_point);

		bool IsTargetable() const override { return true; }

		glm::vec3 GetApproachPoint() const override { return approach_point_; }

	private:
		glm::vec3                   approach_point_;
		bool                        is_dying_ = false;
		float                       dissolve_timer_ = 0.0f;
		bool                        approach_point_set_ = false;
		float                       time_since_last_fire_ = 0.0f;
		float                       fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized
		static constexpr int        kMaxInFlightMissiles = 5;
		std::shared_ptr<ArcadeText> text_ = nullptr;

		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
