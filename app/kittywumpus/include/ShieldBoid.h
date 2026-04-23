#pragma once

#include <memory>
#include "entity.h"
#include "polyhedron.h"

namespace Boidsish {

	enum class ShieldBoidState { WILD, CAPTURED, INTERCEPTING };

	class ShieldBoid : public Entity<Polyhedron> {
	public:
		ShieldBoid(int id = 0, const glm::vec3& orbit_point = glm::vec3(0.0f));

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void SetTarget(std::shared_ptr<EntityBase> target);
		void SetOrbitPoint(const glm::vec3& point);
		ShieldBoidState GetState() const { return state_; }
		void Capture(std::shared_ptr<EntityBase> player);

	private:
		ShieldBoidState state_ = ShieldBoidState::WILD;
		std::shared_ptr<EntityBase> target_ = nullptr;
		glm::vec3 orbit_point_ = glm::vec3(0.0f);
		float orbit_angle_ = 0.0f;
		float orbit_radius_ = 15.0f;
		float orbit_speed_ = 2.0f;
		float vertical_offset_ = 0.0f;
		float spiral_speed_ = 1.0f;

		std::shared_ptr<EntityBase> intercept_target_ = nullptr;

		void UpdateWild(const EntityHandler& handler, float delta_time);
		void UpdateCaptured(const EntityHandler& handler, float time, float delta_time);
		void UpdateIntercepting(const EntityHandler& handler, float delta_time);

		std::shared_ptr<EntityBase> FindThreat(const EntityHandler& handler, const glm::vec3& player_pos);
	};

} // namespace Boidsish
