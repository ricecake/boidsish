#include "GuidedMissile.h"
#include "DamageableEntity.h"
#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h" // For Visualizer access in EntityHandler
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	GuidedMissile::GuidedMissile(int id, Vector3 pos):
		Entity<Model>(id, "assets/Missile.obj", true),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(0.0f),
		eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		SetVelocity(0, 0, 0);
		SetTrailLength(500);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		UpdateShape();
	}

	void GuidedMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		auto pos = GetPosition();

		if (exploded_) {
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		if (lived_ >= lifetime_) {
			Explode(handler, false);
			return;
		}
		auto [height, norm] = handler.vis->GetTerrainPointProperties(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler, false);
			return;
		}

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 170.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			orientation_ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			forward_speed_ += kAcceleration * delta_time;
			if (forward_speed_ > kMaxSpeed) {
				forward_speed_ = kMaxSpeed;
			}
		} else {
			const float kTurnSpeed = 4.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (targets.empty()) {
				rotational_velocity_ = glm::vec3(0.0f);
			} else {
				auto plane = targets[0];

				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
					plane->TriggerDamage();
					return;
				}

				Vector3   target_vec = (plane->GetPosition() - GetPosition()).Normalized();
				glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);
				glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;

				glm::vec3 target_rot_velocity = glm::vec3(0.0f);
				target_rot_velocity.y = target_dir_local.x * kTurnSpeed;
				target_rot_velocity.x = -target_dir_local.y * kTurnSpeed;

				rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

				if (lived_ <= 1.5f) {
					std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
					glm::vec3                             error_vector(0.1f * dist(eng_), dist(eng_), 0);
					rotational_velocity_ += error_vector * delta_time;
				}

				const auto* terrain_generator = handler.GetTerrainGenerator();
				if (terrain_generator) {
					const float reaction_distance = 100.0f;
					float       hit_dist = 0.0f;

					Vector3 vel_vec = GetVelocity();
					if (vel_vec.MagnitudeSquared() > 1e-6) {
						glm::vec3 origin = {GetPosition().x, GetPosition().y, GetPosition().z};
						glm::vec3 dir = {vel_vec.x, vel_vec.y, vel_vec.z};
						dir = glm::normalize(dir);

						if (terrain_generator->Raycast(origin, dir, reaction_distance, hit_dist)) {
							auto hit_coord = vel_vec.Normalized() * hit_dist;
							auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(
								hit_coord.x,
								hit_coord.z
							);

							const float avoidance_strength = 20.0f;
							const float kUpAlignmentThreshold = 0.5f;
							float force_magnitude = avoidance_strength * (1.0f - ((10 + hit_dist) / reaction_distance));

							glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
							auto      away = terrain_normal;
							if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
								away = local_up;
							}
							glm::vec3 avoidance_force = away * force_magnitude;
							glm::vec3 avoidance_local = glm::inverse(orientation_) * avoidance_force;
							rotational_velocity_.y += avoidance_local.x * avoidance_strength * delta_time;
							rotational_velocity_.x += avoidance_local.y * avoidance_strength * delta_time;
						}
					}
				}
			}
		}

		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_;
		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
	}

	void GuidedMissile::UpdateShape() {
		Entity<Model>::UpdateShape();
		if (shape_) {
			shape_->SetRotation(orientation_);
		}
	}

	void GuidedMissile::Explode(const EntityHandler& handler, bool hit_target) {
		if (exploded_)
			return;

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0),
				glm::vec3(0, 0, 0),
				-1,
				2.0f
			);
		});

		// --- Area-of-Effect Damage ---
		const float explosionRadius = 50.0f;
		const float maxDamage = 100.0f;

		auto entities = handler.GetEntitiesByType<DamageableEntity>();
		for (auto& entity : entities) {
			float distance = (entity->GetPosition() - pos).Magnitude();
			if (distance < explosionRadius) {
				float damage = maxDamage * (1.0f - (distance / explosionRadius));
				entity->ApplyDamage(handler, damage);
			}
		}

		handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
			if (exhaust) {
				exhaust->SetLifetime(0.25f);
				exhaust->SetLived(0.0f);
			}
		});

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));

		if (hit_target) {
			SetSize(100);
			SetColor(1, 0, 0, 0.33f);
		}
	}

} // namespace Boidsish
