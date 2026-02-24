#include "KittywumpusPlane.h"

#include "Beam.h"
#include "CatBomb.h"
#include "CatMissile.h"
#include "KittywumpusHandler.h" // For selected_weapon
#include "Tracer.h"
#include "entity.h"
#include "graphics.h"
#include "terrain_generator_interface.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

KittywumpusPlane::KittywumpusPlane(int id):
	Entity<Model>(id, "assets/Mesh_Cat.obj", true),
	orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
	rotational_velocity_(glm::vec3(0.0f)),
	forward_speed_(20.0f),
	beam_id_(-1),
	beam_spawn_queued_(false) {
	rigid_body_.linear_friction_ = 0.01f;
	rigid_body_.angular_friction_ = 0.01f;

	SetTrailLength(10);
	SetTrailIridescence(true);

	SetColor(1.0f, 0.5f, 0.0f);
	shape_->SetScale(glm::vec3(0.04f));
	std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
		glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
	);
	SetPosition(0, 4, 0);

	// Initial velocity for a nice takeoff
	SetVelocity(Vector3(0, 0, 20));

	// Correct the initial orientation to match the model's alignment
	orientation_ = glm::angleAxis(glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	UpdateShape();
}

void KittywumpusPlane::SetController(std::shared_ptr<KittywumpusInputController> controller) {
	controller_ = controller;
}

bool KittywumpusPlane::CanLand(float height_above_ground) const {
	return state_ == PlaneState::ALIVE && height_above_ground < kLandingHeightThreshold;
}

void KittywumpusPlane::BeginLanding() {
	if (state_ != PlaneState::ALIVE) return;

	// Stop physics
	forward_speed_ = 0.0f;
	SetVelocity(Vector3(0, 0, 0));
	rotational_velocity_ = glm::vec3(0.0f);

	// Store current orientation for reference
	landed_orientation_ = orientation_;
	landed_position_ = GetPosition().Toglm();

	// Set grounded state
	is_grounded_ = true;
	state_ = PlaneState::LANDED;

	// Hide the plane model while in FPS mode
	if (shape_) {
		shape_->SetHidden(true);
	}
	SetTrailLength(0);
}

void KittywumpusPlane::BeginTakeoff(float yaw_degrees, Visualizer& viz) {
	if (state_ != PlaneState::LANDED) return;

	// Constants for terrain clearance check
	constexpr float kRaycastDistance = 100.0f;
	constexpr float kPitchIncrementDegrees = 15.0f;
	constexpr float kMaxPitchDegrees = 60.0f;
	constexpr float kYawIncrementDegrees = 15.0f;
	constexpr int kMaxYawAttempts = 24; // 360 degrees / 15 = 24 attempts

	// Boost player higher above ground first
	float boosted_y = landed_position_.y + kTakeoffBoostHeight;
	glm::vec3 start_pos(landed_position_.x, boosted_y, landed_position_.z);

	auto terrain = viz.GetTerrain();

	// Find a clear takeoff direction
	// Priority: increment pitch first (0 to 60), then rotate yaw and repeat
	float final_yaw = yaw_degrees;
	float final_pitch = 0.0f;
	bool found_clear = false;

	for (int yaw_attempt = 0; yaw_attempt < kMaxYawAttempts && !found_clear; ++yaw_attempt) {
		// Calculate test yaw (alternate left/right: 0, +15, -15, +30, -30, ...)
		float test_yaw = yaw_degrees;
		if (yaw_attempt > 0) {
			int direction = (yaw_attempt % 2 == 1) ? 1 : -1;
			int step = (yaw_attempt + 1) / 2;
			test_yaw = yaw_degrees + direction * step * kYawIncrementDegrees;
		}

		// Try increasing pitch angles (0, 15, 30, 45, 60)
		for (float test_pitch = 0.0f; test_pitch <= kMaxPitchDegrees; test_pitch += kPitchIncrementDegrees) {
			float pitch_rad = glm::radians(test_pitch);
			float yaw_rad = glm::radians(test_yaw);

			glm::quat test_yaw_q = glm::angleAxis(yaw_rad, glm::vec3(0, 1, 0));
			glm::quat test_pitch_q = glm::angleAxis(-pitch_rad, glm::vec3(1, 0, 0)); // Negative for nose up
			glm::quat test_orient = test_yaw_q * test_pitch_q;

			glm::vec3 forward_dir = glm::normalize(test_orient * glm::vec3(0.0f, 0.0f, -1.0f));

			// Use built-in terrain raycast
			float hit_distance = 0.0f;
			bool hit = terrain && terrain->Raycast(start_pos, forward_dir, kRaycastDistance, hit_distance);

			if (!hit) {
				// No collision - this direction is clear
				final_yaw = test_yaw;
				final_pitch = test_pitch;
				found_clear = true;
				break;
			}
		}
	}

	// Build final orientation
	float pitch_rad = glm::radians(final_pitch);
	float yaw_rad = glm::radians(final_yaw);

	glm::quat yaw_q = glm::angleAxis(yaw_rad, glm::vec3(0, 1, 0));
	glm::quat pitch_q = glm::angleAxis(-pitch_rad, glm::vec3(1, 0, 0)); // Negative for nose up
	orientation_ = yaw_q * pitch_q;
	rigid_body_.SetOrientation(orientation_);

	// Set position with boost
	SetPosition(start_pos.x, start_pos.y, start_pos.z);

	// Set initial velocity in forward direction with upward component
	glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 velocity = forward_dir * kTakeoffSpeed + glm::vec3(0.0f, kTakeoffBoostHeight, 0.0f);
	SetVelocity(Vector3(velocity.x, velocity.y, velocity.z));
	forward_speed_ = kTakeoffSpeed;

	// Start heading lock timer to prevent player input briefly
	heading_lock_timer_ = kHeadingLockDuration;

	// Restore flight state
	is_grounded_ = false;
	state_ = PlaneState::ALIVE;

	// Show the plane model again
	if (shape_) {
		shape_->SetHidden(false);
	}
	SetTrailLength(10);
}

void KittywumpusPlane::SetLandedPosition(const glm::vec3& pos) {
	landed_position_ = pos;
	if (is_grounded_) {
		SetPosition(pos.x, pos.y, pos.z);
	}
}

void KittywumpusPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
	if (!controller_)
		return;

	// If grounded/landed, skip flight physics
	if (state_ == PlaneState::LANDED) {
		SetVelocity(Vector3(0, 0, 0));
		return;
	}

	if (state_ == PlaneState::DEAD) {
		SetVelocity(Vector3(0, 0, 0));
		return;
	}

	if (state_ == PlaneState::ALIVE && health_ <= 0) {
		state_ = PlaneState::DYING;
	}

	// Update heading lock timer (used during takeoff)
	if (heading_lock_timer_ > 0.0f) {
		heading_lock_timer_ -= delta_time;
	}

	// --- Handle Super Speed State Machine ---
	const float kBuildupDuration = 1.0f;
	const float kTaperingSpeed = 2.0f;

	if (controller_->super_speed) {
		if (super_speed_state_ == SuperSpeedState::NORMAL || super_speed_state_ == SuperSpeedState::TAPERING) {
			super_speed_state_ = SuperSpeedState::BUILDUP;
			super_speed_timer_ = kBuildupDuration;
		} else if (super_speed_state_ == SuperSpeedState::BUILDUP) {
			super_speed_timer_ -= delta_time;
			if (super_speed_timer_ <= 0.0f) {
				super_speed_state_ = SuperSpeedState::ACTIVE;
				super_speed_intensity_ = 5.0f;
				SetTrailRocket(true);
				handler.EnqueueVisualizerAction([&handler]() { handler.vis->SetCameraShake(0.5f, 10.0f); });
			}
			forward_speed_ = glm::mix(forward_speed_, 0.0f, 1.0f - exp(-delta_time * 5.0f));
		}
	} else {
		if (super_speed_state_ == SuperSpeedState::ACTIVE || super_speed_state_ == SuperSpeedState::BUILDUP) {
			super_speed_state_ = SuperSpeedState::TAPERING;
			SetTrailRocket(false);
			handler.EnqueueVisualizerAction([&handler]() { handler.vis->SetCameraShake(0.0f, 0.0f); });
		}

		if (super_speed_state_ == SuperSpeedState::TAPERING) {
			super_speed_intensity_ -= kTaperingSpeed * delta_time;
			if (super_speed_intensity_ <= 0.0f) {
				super_speed_intensity_ = 0.0f;
				super_speed_state_ = SuperSpeedState::NORMAL;
			}
		}
	}

	handler.EnqueueVisualizerAction([&handler, intensity = super_speed_intensity_]() {
		handler.vis->SetSuperSpeedIntensity(intensity);
	});

	// --- Constants for flight model ---
	const float kPitchSpeed = 1.5f;
	const float kYawSpeed = 1.5f;
	const float kRollSpeed = 3.0f;
	const float kCoordinatedTurnFactor = 0.8f;
	const float kAutoLevelSpeed = 1.5f;
	const float kDamping = 2.5f;

	const float kBaseSpeed = 60.0f;
	const float kBoostSpeed = 120.0f;
	const float kBreakSpeed = 10.0f;
	const float kBoostAcceleration = 100.0f;
	const float kSpeedDecay = 30.0f;

	auto pos = GetPosition();

	// Handle Beam weapon (Weapon 3)
	Beam* my_beam = nullptr;
	if (beam_id_ >= 0) {
		auto ent = handler.GetEntity(beam_id_);
		my_beam = dynamic_cast<Beam*>(ent.get());
		if (!my_beam || my_beam->GetOwnerId() != id_) {
			my_beam = nullptr;
			beam_id_ = -1;
		}
	}

	if (!my_beam) {
		auto beams = handler.GetEntitiesByType<Beam>();
		for (auto b : beams) {
			if (b->GetOwnerId() == id_) {
				my_beam = b;
				beam_id_ = b->GetId();
				beam_spawn_queued_ = false;
				break;
			}
		}
	}

	if (kittywumpus_selected_weapon == 3) {
		if (!my_beam && !beam_spawn_queued_) {
			handler.QueueAddEntity<Beam>(id_);
			beam_spawn_queued_ = true;
		} else if (my_beam) {
			my_beam->SetSelected(true);
			my_beam->SetRequesting(controller_->fire);
			my_beam->SetOffset(glm::vec3(0, 0, -0.5f));
		}
	} else if (my_beam) {
		my_beam->SetSelected(false);
		my_beam->SetRequesting(false);
	}

	auto [height, norm] = handler.vis->GetTerrainPropertiesAtPoint(pos.x, pos.z);
	if (pos.y < height) {
		if (state_ == PlaneState::DYING) {
			if (health_ < -10.0f) {
				state_ = PlaneState::DEAD;
				handler.EnqueueVisualizerAction(
					[&handler, pos = GetPosition().Toglm(), effect = dying_fire_effect_]() {
						if (handler.vis) {
							handler.vis->CreateExplosion(pos, 5.0f);
							if (effect) {
								effect->SetActive(false);
								effect->SetLifetime(0.1f);
							}
						}
					}
				);
				if (shape_) {
					shape_->SetHidden(true);
				}
				if (auto* kw_handler = dynamic_cast<const KittywumpusHandler*>(&handler)) {
					kw_handler->OnPlaneDeath(kw_handler->GetScore());
				}
				SetVelocity(Vector3(0, 0, 0));
				return;
			}
		}

		TriggerDamage();
		auto newPos = glm::vec3{pos.x, height, pos.z} + norm * 0.1f;
		SetPosition(newPos);
		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
		auto      new_forward = glm::reflect(forward_dir, norm);
		orientation_ = glm::lookAt(pos.Toglm(), pos.Toglm() + new_forward, glm::vec3(0, 1, 0));
		forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_ * 0.150f;
		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));

		return;
	}

	if (state_ == PlaneState::DYING) {
		std::lock_guard<std::mutex> lock(effect_mutex_);
		if (!dying_fire_effect_) {
			fire_effect_timer_ -= delta_time;
			if (fire_effect_timer_ <= 0) {
				handler.EnqueueVisualizerAction([this, &handler, p = pos.Toglm()]() {
					if (handler.vis) {
						auto                        effect = handler.vis->AddFireEffect(p, FireEffectStyle::Fire);
						std::lock_guard<std::mutex> lock(this->effect_mutex_);
						this->dying_fire_effect_ = effect;
					}
				});
				fire_effect_timer_ = 1.0f;
			}
		} else {
			handler.EnqueueVisualizerAction([effect = dying_fire_effect_, p = pos.Toglm()]() {
				effect->SetPosition(p);
			});
		}
	}

	// --- Handle Rotational Input ---
	// During heading lock (takeoff), ignore player rotation input
	glm::vec3 target_rot_velocity = glm::vec3(0.0f);
	bool heading_locked = heading_lock_timer_ > 0.0f;

	if (!heading_locked) {
		if (controller_->pitch_up)
			target_rot_velocity.x += kPitchSpeed;
		if (controller_->pitch_down)
			target_rot_velocity.x -= kPitchSpeed;
		if (controller_->yaw_left)
			target_rot_velocity.y += kYawSpeed;
		if (controller_->yaw_right)
			target_rot_velocity.y -= kYawSpeed;
		if (controller_->roll_left)
			target_rot_velocity.z += kRollSpeed;
		if (controller_->roll_right)
			target_rot_velocity.z -= kRollSpeed;
	}

	// --- Coordinated Turn (Banking) ---
	target_rot_velocity.z += target_rot_velocity.y * kCoordinatedTurnFactor;

	// --- Terrain Avoidance in Super Speed ---
	if (super_speed_state_ == SuperSpeedState::ACTIVE) {
		auto pos = GetPosition();
		auto [height, norm] = handler.vis->GetTerrainPropertiesAtPoint(pos.x, pos.z);
		(void)norm;
		float safety_height = height + 10.0f;
		if (pos.y < safety_height) {
			float factor = (safety_height - pos.y) / 10.0f;
			target_rot_velocity.x += kPitchSpeed * factor * 2.0f;
		}
	}

	// --- Auto-leveling ---
	if (state_ == PlaneState::DYING) {
		target_rot_velocity *= 0.2f;
		target_rot_velocity.z += 0.75f * spiral_intensity_ * sin(time / 3);
		target_rot_velocity.x += 0.5f * spiral_intensity_ * sin(time / 5);
	}

	if (!controller_->pitch_up && !controller_->pitch_down && !controller_->yaw_left && !controller_->yaw_right &&
	    !controller_->roll_left && !controller_->roll_right) {
		glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 plane_forward_world = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);

		if (state_ == PlaneState::DYING) {
			world_up = glm::normalize(glm::vec3(0.0f, 1.0f, -0.40f));
		}

		glm::vec3 world_up_in_local = glm::inverse(orientation_) * world_up;

		float pitch_error = glm::asin(glm::dot(plane_forward_world, world_up));
		float roll_error = atan2(world_up_in_local.x, world_up_in_local.y);

		if (abs(glm::dot(plane_forward_world, world_up)) > 0.99f) {
			roll_error = 0.0f;
		}

		target_rot_velocity.x -= pitch_error * kAutoLevelSpeed;
		target_rot_velocity.z -= roll_error * kAutoLevelSpeed;
	}

	if (my_beam && (my_beam->IsCharging() || my_beam->IsFiring() || my_beam->IsShrinking())) {
		target_rot_velocity = glm::vec3(0.0f);
		rotational_velocity_ = glm::vec3(0.0f);
	}

	rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

	glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));
	orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta * roll_delta);
	rigid_body_.SetOrientation(orientation_);

	if (super_speed_state_ == SuperSpeedState::ACTIVE) {
		forward_speed_ = kBoostSpeed * 3.0f;
	} else if (controller_->boost) {
		forward_speed_ += kBoostAcceleration * delta_time;
		if (forward_speed_ > kBoostSpeed)
			forward_speed_ = kBoostSpeed;
	} else if (controller_->brake) {
		forward_speed_ -= kBoostAcceleration * delta_time;
		if (forward_speed_ < kBreakSpeed)
			forward_speed_ = kBreakSpeed;
	} else {
		if (forward_speed_ > kBaseSpeed) {
			forward_speed_ -= kSpeedDecay * delta_time;
			if (forward_speed_ < kBaseSpeed)
				forward_speed_ = kBaseSpeed;
		} else if (forward_speed_ < kBaseSpeed) {
			forward_speed_ += kSpeedDecay * delta_time;
			if (forward_speed_ > kBaseSpeed)
				forward_speed_ = kBaseSpeed;
		}
	}

	glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 new_velocity = forward_dir * forward_speed_;
	SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));

	time_to_fire -= delta_time;
	if (controller_->fire && time_to_fire <= 0) {
		switch (kittywumpus_selected_weapon) {
		case 0:
			handler.QueueAddEntity<CatMissile>(
				pos,
				orientation_,
				glm::normalize(glm::vec3(fire_left ? -1.0f : 1.0f, -1.0f, 0.0f)),
				GetVelocity(),
				fire_left
			);
			time_to_fire = 0.25f;
			fire_left = !fire_left;
			if (fire_left) {
				time_to_fire = 1.0f;
			}
			break;
		case 1:
			handler.QueueAddEntity<CatBomb>(pos, orientation_ * glm::vec3(0, -1, 0), GetVelocity());
			time_to_fire = 1.25f;
			break;
		case 2: {
			glm::vec3 forward = orientation_ * glm::vec3(0, 0, -1);
			glm::vec3 right = orientation_ * glm::vec3(1, 0, 0);

			float     tracer_speed = 600.0f;
			glm::vec3 tracer_vel = GetVelocity().Toglm() + forward * tracer_speed;

			glm::vec3 color = weapon_toggle_ ? glm::vec3(1.0f, 0.2f, 0.0f) : glm::vec3(1.0f, 0.6f, 0.0f);
			weapon_toggle_ = !weapon_toggle_;

			glm::vec3 offset = right * (fire_left ? -0.5f : 0.5f);
			fire_left = !fire_left;

			handler.QueueAddEntity<Tracer>(pos.Toglm() + offset, orientation_, tracer_vel, color, id_);

			time_to_fire = 0.05f;
			break;
		}
		case 3:
			// Beam weapon is handled above
			break;
		}
	}

	if (controller_->chaff) {
		chaff_timer_ = 0.5f;
		handler.EnqueueVisualizerAction([&handler, pos, forward_dir]() {
			handler.vis->AddFireEffect(
				pos.Toglm() - forward_dir,
				FireEffectStyle::Glitter,
				glm::normalize(-1 * forward_dir),
				glm::normalize(-5 * forward_dir),
				1500,
				1.0f
			);
		});
	}

	if (chaff_timer_ > 0.0f) {
		chaff_timer_ -= delta_time;
	}
}

void KittywumpusPlane::UpdateShape() {
	Entity<Model>::UpdateShape();
	if (shape_) {
		shape_->SetRotation(orientation_);
	}
}

void KittywumpusPlane::OnHit(const EntityHandler& handler, float damage) {
	(void)handler;
	health_ -= damage;
	damage_pending_++;
	if (state_ == PlaneState::DYING) {
		spiral_intensity_ += 1.0f;
	}
}

void KittywumpusPlane::TriggerDamage() {
	health_ -= 5;
	damage_pending_++;
	if (state_ == PlaneState::DYING) {
		spiral_intensity_ += (std::abs(health_) - 1) / (std::max(std::abs(health_), 1.0f));
	}
}

bool KittywumpusPlane::IsDamagePending() {
	return damage_pending_ > 0;
}

void KittywumpusPlane::AcknowledgeDamage() {
	damage_pending_--;
}

float KittywumpusPlane::GetHealth() const {
	return health_;
}

float KittywumpusPlane::GetShield() const {
	return shield_;
}

void KittywumpusPlane::ResetState() {
	health_ = 100.0f;
	shield_ = 100.0f;
	state_ = PlaneState::ALIVE;
	damage_pending_ = 0;
	is_grounded_ = false;
	heading_lock_timer_ = 0.0f;
	chaff_timer_ = 0.0f;
	forward_speed_ = 20.0f;
	spiral_intensity_ = 1.0f;
	super_speed_state_ = SuperSpeedState::NORMAL;
	super_speed_timer_ = 0.0f;
	super_speed_intensity_ = 0.0f;
	dying_fire_effect_.reset();

	if (shape_) {
		shape_->SetHidden(false);
	}
	SetTrailLength(10);
	SetTrailRocket(false);
}

} // namespace Boidsish
