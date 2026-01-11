#pragma once

#include <memory>

#include "PaperPlaneInputController.h"
#include "DamageableEntity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class PaperPlane: public DamageableEntity {
	public:
		PaperPlane(int id = 0);

		void SetController(std::shared_ptr<PaperPlaneInputController> controller);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;
		void OnDamage(const EntityHandler& handler, float damage) override;

		void TriggerDamage();
		bool IsDamagePending();
		void AcknowledgeDamage();

	private:
		std::shared_ptr<PaperPlaneInputController> controller_;
		glm::quat                                  orientation_;
		glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
		float                                      forward_speed_;
		float                                      time_to_fire = 0.25f;
		bool                                       fire_left = true;
		int                                        damage_pending_ = 0;
	};

} // namespace Boidsish
