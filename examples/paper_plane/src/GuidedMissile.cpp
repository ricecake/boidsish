#include "GuidedMissile.h"

#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h" // For Visualizer access in EntityHandler
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	GuidedMissile::GuidedMissile(int id, Vector3 pos):
		Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		SetVelocity(0, 50, 0); // Initial upward velocity
		SetTrailLength(500);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		SetOrientToVelocity(true);
	}

	void GuidedMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		logger::LOG(rigid_body_.ToString());
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
			rigid_body_.AddRelativeForce({0, 0, 1 * kAcceleration});
		} else {
			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (targets.empty()) {
				return;
			}
			auto plane = targets[0];

			if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
				Explode(handler, true);
				plane->TriggerDamage();
				return;
			}

			// Proportional navigation
			Vector3 r = plane->GetPosition() - GetPosition();
			Vector3 v = plane->GetVelocity() - GetVelocity();
			Vector3 r_norm = r.Normalized();
			Vector3 v_norm = v.Normalized();

			float N = 4.0f; // Proportional navigation constant
			Vector3 omega = r.Cross(v) * (N / r.Dot(r));

			// Apply torque to align velocity with the target
			Vector3 forward = rigid_body_.GetOrientation() * glm::vec3(0, 0, 1);
			Vector3 torque = forward.Cross(omega.Normalized());
			rigid_body_.AddTorque(torque * 100.0f);

			// Apply forward thrust
			rigid_body_.AddRelativeForce({0, 0, 1 * kAcceleration});
		}

		// Clamp velocity to max speed
		if (GetVelocity().Magnitude() > kMaxSpeed) {
			rigid_body_.SetLinearVelocity(GetVelocity().Normalized() * kMaxSpeed);
		}

		// Terrain avoidance
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
					Vector3 forward = rigid_body_.GetOrientation() * glm::vec3(0, 0, 1);
					Vector3 torque = forward.Cross(Vector3(avoidance_force.x, avoidance_force.y, avoidance_force.z));
					rigid_body_.AddTorque(torque * 10.0f * delta_time);
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
