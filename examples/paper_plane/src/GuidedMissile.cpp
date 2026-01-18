#include "GuidedMissile.h"

#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h" // For Visualizer access in EntityHandler
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	GuidedMissile::GuidedMissile(int id, Vector3 pos):
		Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		// SetPosition(pos.x, pos.y, pos.z);
		// SetVelocity(0, 0, 0);

		auto orientation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		SetOrientToVelocity(false);
		SetPosition(pos.x, pos.y, pos.z);

		rigid_body_.SetOrientation(orientation);
		rigid_body_.SetLinearVelocity(glm::vec3(0, 100, 0));


		SetTrailLength(500);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);

		// rigid_body_.SetOrientation(orientation);
		// rigid_body_.SetPosition(pos.Toglm());
		// rigid_body_.SetLinearVelocity(ObjectToWorld(glm::vec3(0, 0, 100)));
	}

	void GuidedMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		auto pos = GetPosition();

		if (!launch_sound_) {
			launch_sound_ = handler.vis
								->AddSoundEffect("assets/sam_launch.wav", pos.Toglm(), GetVelocity().Toglm(), 30.0f);
		}

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
		if (pos.y < height) {
			Explode(handler, false);
			return;
		}

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 170.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			rigid_body_.AddRelativeForce(glm::vec3(0, 0, 150));
		} else {
			const float kTurnSpeed = 100.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (targets.empty()) {
			} else {
				auto  plane = targets[0];
				auto& r = rigid_body_;

				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
					// plane->TriggerDamage();
					return;
				}

				r.AddRelativeForce(glm::vec3(0, 0, 300));

				Vector3   target_vec = (plane->GetPosition() - GetPosition()).Normalized();
				glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);

				glm::vec3 target_dir_local = WorldToObject(target_dir_world);
				glm::vec3 P = glm::vec3(0, 0, 1);
				glm::vec3 torque = glm::cross(P, target_dir_local);
				r.AddRelativeTorque(torque * kTurnSpeed);

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

							glm::vec3 target_dir_local = WorldToObject(away);
							glm::vec3 P = glm::vec3(0, 0, 1);
							glm::vec3 torque = glm::cross(P, target_dir_local);
							r.AddRelativeTorque(torque * kTurnSpeed);
						}
					}
				}
			}
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

		handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
			if (exhaust) {
				exhaust->SetLifetime(0.25f);
				exhaust->SetLived(0.0f);
			}
		});

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));
		explode_sound_ = handler.vis
							 ->AddSoundEffect("assets/rocket_explosion.wav", pos.Toglm(), GetVelocity().Toglm(), 20.0f);

		if (hit_target) {
			SetSize(100);
			SetColor(1, 0, 0, 0.33f);
		}
	}

} // namespace Boidsish
