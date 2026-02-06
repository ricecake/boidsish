#include "FighterPlane.h"

#include "Bullet.h"
#include "GuidedMissileLauncher.h"
#include "PaperPlane.h"
#include "graphics.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	static glm::vec3 CalculateSteeringTorque(
		const glm::vec3& current_forward,
		const glm::vec3& desired_direction,
		const glm::vec3& current_angular_velocity,
		float            kP,
		float            kD
	) {
		glm::vec3 error_vector = glm::cross(current_forward, desired_direction);
		glm::vec3 derivative_term = current_angular_velocity;
		return (error_vector * kP) - (derivative_term * kD);
	}

	FighterPlane::FighterPlane(int id, int launcher_id, Vector3 pos):
		Entity<Model>(id, "assets/dogplane.obj", true), launcher_id_(launcher_id), eng_(rd_()) {
		SetPosition(pos);
		SetColor(0.8f, 0.2f, 0.2f, 1.0f);
		SetSize(35.0f);
		SetTrailLength(100);
		SetTrailPBR(true);
		SetTrailRoughness(0.2f);
		SetTrailMetallic(0.8f);

		rigid_body_.linear_friction_ = 1.0f;
		rigid_body_.angular_friction_ = 5.0f;

		shape_->SetScale(glm::vec3(5.0f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		shape_->SetInstanced(true);

		UpdateShape();
	}

	void FighterPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		lived_ += delta_time;

		if (exploded_) {
			if (lived_ > 2.0f) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		auto pos = GetPosition().Toglm();
		auto [terrain_h, terrain_norm] = handler.GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);

		if (state_ == State::CRASHING) {
			spiral_timer_ += delta_time;
			rigid_body_.AddRelativeTorque(glm::vec3(200.0f, 50.0f, 400.0f));
			rigid_body_.AddForce(glm::vec3(0, -150.0f, 0)); // Gravity

			if (pos.y <= terrain_h + 1.0f) {
				Explode(handler);
			}
			return;
		}

		// Check if launcher still exists
		auto launcher = std::dynamic_pointer_cast<GuidedMissileLauncher>(handler.GetEntity(launcher_id_));
		if (!launcher) {
			handler.QueueRemoveEntity(id_);
			return;
		}

		// Check for player
		auto        planes = handler.GetEntitiesByType<PaperPlane>();
		PaperPlane* player = nullptr;
		if (!planes.empty())
			player = planes[0];

		float dist_to_player = player ? glm::distance(pos, player->GetPosition().Toglm()) : 1e10f;

		// State transitions
		if (state_ == State::CIRCLING) {
			if (dist_to_player < kEngagementRadius) {
				state_ = State::ENGAGING;
			}
		} else if (state_ == State::ENGAGING) {
			if (!player || dist_to_player > kDisengagementRadius) {
				state_ = State::CIRCLING;
			}
		}

		glm::vec3 my_fwd = ObjectToWorld(glm::vec3(0, 0, -1));
		glm::vec3 desired_dir_world = my_fwd;
		float     target_speed = kCirclingSpeed;

		if (state_ == State::CIRCLING) {
			glm::vec3 l_pos = launcher->GetPosition().Toglm();
			glm::vec3 to_launcher = l_pos - pos;
			float     dist_to_launcher = glm::length(to_launcher);

			if (dist_to_launcher > 0.001f) {
				glm::vec3 to_launcher_n = to_launcher / dist_to_launcher;
				glm::vec3 orbit = glm::cross(glm::vec3(0, 1, 0), to_launcher_n);

				// Aim for a point on the orbit circle
				glm::vec3 to_pos = pos - l_pos;
				float     dist_to_pos = glm::length(to_pos);
				glm::vec3 to_pos_n = (dist_to_pos > 0.001f) ? to_pos / dist_to_pos : glm::vec3(1, 0, 0);

				glm::vec3 target_orbit_pos = l_pos + to_pos_n * kCirclingRadius + orbit * 50.0f;
				glm::vec3 to_target = target_orbit_pos - pos;
				if (glm::length(to_target) > 0.001f) {
					desired_dir_world = glm::normalize(to_target);
				}
			}
			target_speed = kCirclingSpeed;
		} else if (state_ == State::ENGAGING && player) {
			glm::vec3 p_pos = player->GetPosition().Toglm();
			glm::vec3 to_player = p_pos - pos;
			if (glm::length(to_player) > 0.001f) {
				desired_dir_world = glm::normalize(to_player);
			}
			target_speed = kEngagingSpeed;

			// Fire gun
			fire_timer_ += delta_time;
			if (fire_timer_ > kFireInterval) {
				float dot = glm::dot(my_fwd, desired_dir_world);
				if (dot > 0.95f && dist_to_player < 300.0f) {
					handler.QueueAddEntity<Bullet>(GetPosition(), rigid_body_.GetOrientation(), GetVelocity(), true);
					fire_timer_ = 0.0f;
				}
			}
		}

		// Terrain Hugging / Avoidance
		float target_h = terrain_h + 40.0f;
		float h_err = target_h - pos.y;
		desired_dir_world.y += h_err * 0.1f;
		desired_dir_world = glm::normalize(desired_dir_world);

		// Steering
		glm::vec3 desired_dir_local = WorldToObject(desired_dir_world);
		glm::vec3 local_angular_vel = WorldToObject(rigid_body_.GetAngularVelocity());
		glm::vec3 torque = CalculateSteeringTorque(
			glm::vec3(0, 0, -1),
			desired_dir_local,
			local_angular_vel,
			100.0f,
			10.0f
		);

		// Banking
		glm::vec3 world_up(0, 1, 0);
		glm::vec3 bank_axis = glm::cross(my_fwd, world_up);
		if (glm::length(bank_axis) > 0.001f) {
			bank_axis = glm::normalize(bank_axis);
			float     turn_amount = glm::dot(desired_dir_world, bank_axis);
			float     lean_scale = 2.0f;
			glm::vec3 target_up_world = glm::normalize(world_up + bank_axis * turn_amount * lean_scale);
			glm::vec3 target_up_local = WorldToObject(target_up_world);
			glm::vec3 up_error = glm::cross(glm::vec3(0, 1, 0), target_up_local);
			torque.z += (up_error.z * 150.0f) - (local_angular_vel.z * 15.0f);
		}

		rigid_body_.AddRelativeTorque(torque);

		// Thrust
		rigid_body_.AddRelativeForce(glm::vec3(0, 0, -1000.0f));
		glm::vec3 vel = rigid_body_.GetLinearVelocity();
		float     speed = glm::length(vel);
		if (speed > target_speed) {
			rigid_body_.SetLinearVelocity(vel * (target_speed / speed));
		}
	}

	void FighterPlane::ShotDown(const EntityHandler& handler) {
		(void)handler;
		if (state_ == State::CRASHING)
			return;
		state_ = State::CRASHING;
		spiral_timer_ = 0.0f;
		SetTrailRocket(true);
		SetColor(0.5f, 0.5f, 0.5f, 1.0f);
	}

	void FighterPlane::Explode(const EntityHandler& handler) {
		if (exploded_)
			return;
		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(0, 0, 0);
		SetSize(0);

		auto pos = GetPosition().Toglm();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->TriggerComplexExplosion(shape_, glm::vec3(0, 1, 0), 2.0f, FireEffectStyle::Explosion);
			handler.vis->GetTerrain()->AddCrater({pos.x, pos.y, pos.z}, 15.0f, 8.0f, 0.2f, 2.0f);
			handler.vis->AddSoundEffect("assets/rocket_explosion.wav", pos, glm::vec3(0), 20.0f);
		});
	}

} // namespace Boidsish
