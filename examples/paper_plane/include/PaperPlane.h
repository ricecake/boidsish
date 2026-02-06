#pragma once

#include <memory>

#include "PaperPlaneInputController.h"
#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class PaperPlane: public Entity<Model> {
	public:
		PaperPlane(int id = 0);

		void SetController(std::shared_ptr<PaperPlaneInputController> controller);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;

		void TriggerDamage();
		bool IsDamagePending();
		void AcknowledgeDamage();

		float GetHealth() const;
		float GetShield() const;
		bool  IsChaffActive() const { return chaff_timer_ > 0.0f; }

	private:
		std::shared_ptr<PaperPlaneInputController> controller_;
		glm::quat                                  orientation_;
		glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
		float                                      forward_speed_;
		float                                      time_to_fire = 0.25f;
		bool                                       fire_left = true;
		int                                        damage_pending_ = 0;
		float                                      health = 100.0f;
		float                                      shield = 100.0f;
		float                                      chaff_timer_ = 0.0f;

		// Super speed effect state
		enum class SuperSpeedState { NORMAL, BUILDUP, ACTIVE, TAPERING };
		SuperSpeedState super_speed_state_ = SuperSpeedState::NORMAL;
		float           super_speed_timer_ = 0.0f;
		float           super_speed_intensity_ = 0.0f;
	};

} // namespace Boidsish
