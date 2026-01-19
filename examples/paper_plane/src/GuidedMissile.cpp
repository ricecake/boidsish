#include "GuidedMissile.h"

#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h" // For Visualizer access in EntityHandler
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	GuidedMissile::GuidedMissile(int id, Vector3 pos):
		Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		auto orientation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		SetOrientToVelocity(false);
		SetPosition(pos.x, pos.y+0.5f, pos.z);

		rigid_body_.SetOrientation(orientation);
		rigid_body_.SetLinearVelocity(glm::vec3(0, 100, 0));


		SetTrailLength(100);
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
			rigid_body_.AddRelativeForce(glm::vec3(0, 0, 600));
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

				r.AddRelativeForce(glm::vec3(0, 0, 500));


				glm::vec3 velocity = rigid_body_.GetLinearVelocity();
				glm::vec3   target_vec = (plane->GetPosition() - GetPosition()).Toglm();
				glm::vec3 target_dir_world = glm::normalize(target_vec);


				// log10(x/10+1)/2
				std::uniform_real_distribution<float> dist(0.5f, 0.75f);
				auto distance = glm::length(target_vec);
				auto distance_scale = log10(distance/50.0f+1);
				float error_scale = (1.0f - lived_/lifetime_) * distance_scale;
				glm::vec3 right = glm::cross(target_dir_world, glm::vec3(0,1,0));
				glm::vec3 up = glm::cross(right, target_dir_world);
				auto theta = lived_ * (2.0f+2*(1.0f-distance_scale));
				auto offset=(right*cos(theta))+(up*0.75f*sin(theta));

				auto adjusted_target_dir_world = glm::normalize(target_vec + offset*dist(eng_)*distance*error_scale*error_scale);

				glm::vec3 target_dir_local = WorldToObject(adjusted_target_dir_world);
				glm::vec3 P = glm::vec3(0, 0, 1);
				glm::vec3 torque = glm::cross(P, target_dir_local);


/*

use something like this for spiral attacker!
				float error_scale = 1;//1.0f - lived_/lifetime_;
				// log10(x/10+1)/2
				error_scale *= log10(glm::length(target_vec)/25.0f+1);
				glm::vec3 right = glm::cross(target_dir_world, glm::vec3(0,1,0));
				glm::vec3 up = glm::cross(right, target_dir_world);
				auto theta = lived_ * 5.0f/error_scale;
				auto offset=(right*cos(theta))+(up*sin(theta));

				auto adjusted_target_dir_world = glm::normalize(target_vec + offset*200.0f*error_scale);

				glm::vec3 target_dir_local = WorldToObject(adjusted_target_dir_world);
				glm::vec3 P = glm::vec3(0, 0, 1);
				glm::vec3 torque = glm::cross(P, target_dir_local);

*/



				// if (lived_ < 5.0f) {
				// std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
				// glm::vec3                             error_vector = glm::cross(velocity, glm::vec3(dist(eng_), dist(eng_), dist(eng_)));
				// float error_scale = 1.0f - lived_/lifetime_;
				// float disalignment = 1.0f - glm::dot(target_dir_world, glm::normalize(velocity))*0.5f+0.5f;
				// torque = glm::length(torque) * glm::normalize(torque + disalignment * error_scale * error_vector);
				// torque = glm::length(torque) * glm::normalize(torque + error_scale * std::log2(std::min(10.0f, glm::length(target_vec))) * error_vector);
				// torque = glm::length(torque) * glm::normalize(torque + disalignment * error_scale * error_vector);
				// torque = glm::length(torque) * glm::normalize(torque + disalignment * (error_scale/std::log2(std::min(10.0f, glm::length(target_vec)))) * error_vector);
				r.AddRelativeTorque(torque * kTurnSpeed);

					// }



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
		// handler.EnqueueVisualizerAction([=, &handler]() {
		// 	handler.vis->AddFireEffect(
		// 		glm::vec3(pos.x, pos.y, pos.z),
		// 		FireEffectStyle::Explosion,
		// 		glm::vec3(0, 1, 0),
		// 		glm::vec3(0, 0, 0),
		// 		-1,
		// 		2.0f
		// 	);
		// });

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
