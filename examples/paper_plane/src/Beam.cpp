#include "Beam.h"

#include "GuidedMissileLauncher.h"
#include "PaperPlane.h"
#include "graphics.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

	Beam::Beam(int owner_id):
		Entity<Line>(glm::vec3(0.0f), glm::vec3(0.0f), kAimingWidth), owner_id_(owner_id) {
		shape_->SetStyle(Line::Style::LASER);
		shape_->SetHidden(true);
		SetSize(0.0f);                 // Disable physical collision radius
		SetVelocity(Vector3(0, 0, 0)); // Ensure no physical movement
	}

	void Beam::SetRequesting(bool requesting) {
		requesting_ = requesting;
	}

	void Beam::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		auto owner = handler.GetEntity(owner_id_);
		if (!owner) {
			handler.QueueRemoveEntity(GetId());
			return;
		}

		// State Machine
		switch (state_) {
		case State::IDLE:
			if (selected_) {
				state_ = State::AIMING;
				state_timer_ = 0.0f;
				shape_->SetHidden(false);
			}
			break;
		case State::AIMING:
			if (!selected_) {
				state_ = State::IDLE;
				shape_->SetHidden(true);
			} else if (requesting_) {
				state_ = State::FIRING_TRANSITION;
				state_timer_ = 0.0f;
			}
			break;
		case State::FIRING_TRANSITION:
			state_timer_ += delta_time;
			if (state_timer_ >= kTransitionDuration) {
				state_ = State::FIRING_HOLD;
				state_timer_ = 0.0f;
			}
			break;
		case State::FIRING_HOLD:
			state_timer_ += delta_time;
			if (state_timer_ >= kHoldDuration) {
				state_ = State::FIRING_SHRINK;
				state_timer_ = 0.0f;
			}
			break;
		case State::FIRING_SHRINK:
			state_timer_ += delta_time;
			if (state_timer_ >= kShrinkDuration) {
				state_ = State::COOLDOWN;
				state_timer_ = 0.0f;
				shape_->SetHidden(true);
			}
			break;
		case State::COOLDOWN:
			state_timer_ += delta_time;
			if (state_timer_ >= kCooldownDuration) {
				state_ = selected_ ? State::AIMING : State::IDLE;
				state_timer_ = 0.0f;
				if (state_ == State::AIMING) {
					shape_->SetHidden(false);
				}
			}
			break;
		}

		if (state_ == State::IDLE || state_ == State::COOLDOWN) {
			shape_->SetHidden(true);
			return;
		}

		// Update Transform
		SetVelocity(Vector3(0, 0, 0));     // Constantly zero out velocity to prevent physical interactions
		SetPosition(owner->GetPosition()); // Keep entity at owner's position for spatial queries
		glm::vec3 start = owner->GetPosition().Toglm() + owner->ObjectToWorld(offset_);
		glm::vec3 dir = owner->ObjectToWorld(relative_dir_);
		if (glm::dot(dir, dir) < 1e-6) {
			dir = glm::vec3(0, 0, -1);
		} else {
			dir = glm::normalize(dir);
		}

		float     hit_dist = 2000.0f;
		glm::vec3 hit_norm(0, 1, 0);
		bool      hit = handler.RaycastTerrain(start, dir, 2000.0f, hit_dist, hit_norm);

		glm::vec3 end = start + dir * hit_dist;
		shape_->SetStart(start);
		shape_->SetEnd(end);

		// Update Visuals
		float     width = kAimingWidth;
		glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
		float     alpha = 0.4f;

		if (state_ == State::AIMING) {
			width = kAimingWidth;
			color = glm::vec3(1.0f, 1.0f, 0.0f);
			alpha = 0.4f;
		} else if (state_ == State::FIRING_TRANSITION) {
			float t = state_timer_ / kTransitionDuration;
			width = glm::mix(kAimingWidth, kFiringWidth, t);
			color = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), t);
			alpha = glm::mix(0.4f, 1.0f, t);

			auto vis = handler.vis;
			handler.EnqueueVisualizerAction([vis, delta_time]() {
				if (vis)
					vis->SetCameraShake(0.15f, delta_time * 2.0f);
			});
		} else if (state_ == State::FIRING_HOLD) {
			width = kFiringWidth;
			color = glm::vec3(1.0f, 0.0f, 0.0f);
			alpha = 1.0f;

			auto vis = handler.vis;
			handler.EnqueueVisualizerAction([vis, delta_time]() {
				if (vis)
					vis->SetCameraShake(0.4f, delta_time * 2.0f);
			});
		} else if (state_ == State::FIRING_SHRINK) {
			float t = state_timer_ / kShrinkDuration;
			width = glm::mix(kFiringWidth, kShrinkWidth, t);
			color = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), t);
			alpha = 1.0f;

			// Trigger Akira effect and damage at the start of shrink/white phase
			if (state_timer_ <= delta_time) {
				auto vis = handler.vis;
				handler.EnqueueVisualizerAction([vis, end, hit_norm, delta_time]() {
					if (vis) {
						vis->TriggerAkira(end, kDamageRadius);
						vis->CreateShockwave(
							end,
							50.0f,
							kDamageRadius,
							Constants::Class::Akira::DefaultFadeDuration() / 1.5f,
							hit_norm,
							{0, 0, 0},
							-20.0f
						);
						// vis->CreateShockwave(end, 5.0f, kDamageRadius*1.5,
						// Constants::Class::Akira::DefaultFadeDuration()+Constants::Class::Akira::DefaultGrowthDuration(),
						// {0,1,0}, {0,0,0}, 5.0f); vis->CreateExplosion(end, 100.0f);
						vis->SetCameraShake(1.0f, 0.2f);
					}
				});

				// Damage entities
				auto spatial_handler = dynamic_cast<const SpatialEntityHandler*>(&handler);
				if (spatial_handler) {
					auto targets = spatial_handler->GetEntitiesInRadius<EntityBase>(
						Vector3(end.x, end.y, end.z),
						kDamageRadius
					);
					for (auto& target : targets) {
						if (target->GetId() == owner_id_)
							continue;

						target->OnHit(handler, 100.0f); // Large generic damage
					}
				}
			}
		}

		shape_->SetWidth(width);
		shape_->SetColor(color.r, color.g, color.b, alpha);
	}

} // namespace Boidsish
