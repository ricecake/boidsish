#pragma once

#include <memory>
#include <mutex>

#include "KittywumpusInputController.h"
#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

class KittywumpusPlane: public Entity<Model> {
public:
	// Extended PlaneState with LANDED state
	enum class PlaneState { ALIVE, DYING, DEAD, LANDED };

	KittywumpusPlane(int id = 0);

	void SetController(std::shared_ptr<KittywumpusInputController> controller);
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

	// Extended API for Kittywumpus game state
	PlaneState GetPlaneState() const { return state_; }
	glm::quat GetOrientation() const { return orientation_; }
	void SetOrientation(glm::quat orient) { orientation_ = orient; rigid_body_.SetOrientation(orient); }

	// Landing/takeoff API
	bool CanLand(float height_above_ground) const;
	void BeginLanding();
	void BeginTakeoff(float yaw_degrees, class Visualizer& viz);
	bool IsGrounded() const { return is_grounded_; }

	// Update landed position from FPS controller
	void SetLandedPosition(const glm::vec3& pos);
	glm::vec3 GetLandedPosition() const { return landed_position_; }
	glm::quat GetLandedOrientation() const { return landed_orientation_; }

	// Reset for new game
	void ResetState();

	// INTEGRATION_POINT: Add methods for future features
	// - int GetScore() const; (for tracking in-game progression)
	// - void SetObjective(const Objective& obj);
	// - bool HasWeapon(WeaponType type);

private:
	std::shared_ptr<KittywumpusInputController> controller_;
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

	// Kittywumpus-specific state
	bool is_grounded_ = false;
	glm::vec3 landed_position_ = glm::vec3(0.0f);
	glm::quat landed_orientation_ = glm::quat(1, 0, 0, 0);
	float heading_lock_timer_ = 0.0f; // Locks heading during takeoff

	// Landing/takeoff constants
	static constexpr float kLandingHeightThreshold = 2.0f;
	static constexpr float kTakeoffSpeed = 60.0f;
	static constexpr float kTakeoffPitch = 15.0f; // degrees nose up
	static constexpr float kTakeoffBoostHeight = 10.0f; // meters to boost up
	static constexpr float kHeadingLockDuration = 1.5f; // seconds to lock heading
};

} // namespace Boidsish
