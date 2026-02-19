#pragma once

#include <memory>
#include <mutex>

#include "PaperPlaneInputController.h"
#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class PaperPlane: public Entity<Model> {
	public:
		enum class PlaneState { ALIVE, DYING, DEAD };

		PaperPlane(int id = 0);

		void SetController(std::shared_ptr<PaperPlaneInputController> controller);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;

		void TriggerDamage();
		void OnHit(const EntityHandler& handler, float damage) override;
		bool IsDamagePending();
		void AcknowledgeDamage();

		float GetHealth() const;
		float GetShield() const;

		float GetMaxHealth() const { return 100.0f; }

		void AddHealth(float amount) { health_ = std::min(health_ + amount, GetMaxHealth()); }

		bool IsChaffActive() const { return chaff_timer_ > 0.0f; }

	private:
		std::shared_ptr<PaperPlaneInputController> controller_;
		glm::quat                                  orientation_;
		glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
		float                                      forward_speed_;
		float                                      time_to_fire = 0.25f;
		bool                                       fire_left = true;
		bool                                       weapon_toggle_ = false;
		int                                        damage_pending_ = 0;
		float                                      health_ = 100.0f;
		float                                      shield_ = 100.0f;
		float                                      chaff_timer_ = 0.0f;
		PlaneState                                 state_ = PlaneState::ALIVE;
		int                                        beam_id_ = -1;
		bool                                       beam_spawn_queued_ = false;
		float                                      fire_effect_timer_ = 0.0f;
		std::shared_ptr<FireEffect>                dying_fire_effect_;
		float                                      spiral_intensity_ = 1.0f;
		mutable std::mutex                         effect_mutex_;

		// Super speed effect state
		enum class SuperSpeedState { NORMAL, BUILDUP, ACTIVE, TAPERING };
		SuperSpeedState super_speed_state_ = SuperSpeedState::NORMAL;
		float           super_speed_timer_ = 0.0f;
		float           super_speed_intensity_ = 0.0f;
	};

} // namespace Boidsish
