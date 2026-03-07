#include "ChargerEnemy.h"

#include <random>
#include "KittywumpusPlane.h"
#include "KittywumpusHandler.h"
#include "graphics.h"
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	ChargerEnemy::ChargerEnemy(int id, Vector3 pos) : Entity<Model>(id, "assets/dogplane.obj", true) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(4.0f));
		SetColor(1.0f, 1.0f, 1.0f);
		SetOrientToVelocity(true);

		rigid_body_.linear_friction_ = 0.1f;
		rigid_body_.angular_friction_ = 0.1f;

		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
	}

	void ChargerEnemy::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;

		switch (state_) {
			case State::ROAMING:     UpdateRoaming(handler, delta_time);     break;
			case State::POSITIONING: UpdatePositioning(handler, delta_time); break;
			case State::PREPARING:   UpdatePreparing(handler, delta_time);   break;
			case State::CHARGING:    UpdateCharging(handler, delta_time);    break;
			case State::TUMBLING:    UpdateTumbling(handler, delta_time);    break;
			case State::DYING:       UpdateDying(handler, delta_time);       break;
		}

		UpdateShape();

		// Apply visual jitter if preparing
		if (state_ == State::PREPARING) {
			auto current_pos = GetPosition().Toglm();
			shape_->SetPosition(current_pos.x + visual_offset_.x, current_pos.y + visual_offset_.y, current_pos.z + visual_offset_.z);
		}
	}

	void ChargerEnemy::UpdateRoaming(const EntityHandler& handler, float delta_time) {
		auto pos = GetPosition().Toglm();
		auto gen = handler.GetTerrainGenerator();
		if (!gen) return;

		glm::vec3 path_data = gen->GetPathData(pos.x, pos.z);
		float dist_from_spine = path_data.x;
		glm::vec2 gradient = glm::normalize(glm::vec2(path_data.y, path_data.z));

		// Flow along path
		glm::vec2 flow_dir = glm::vec2(-gradient.y, gradient.x);
		glm::vec3 velocity = glm::vec3(flow_dir.x, 0, flow_dir.y) * 25.0f;

		// Correct towards spine if too far
		if (std::abs(dist_from_spine) > 50.0f) {
			glm::vec3 correction = glm::vec3(-gradient.x, 0, -gradient.y) * dist_from_spine * 0.2f;
			velocity += correction;
		}

		// Ground clamping
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(pos.x, pos.z);
		float target_y = h + 2.0f;
		if (pos.y < h) pos.y = h; // Snap up if underground
		pos.y = glm::mix(pos.y, target_y, delta_time * 10.0f);

		SetPosition(pos.x, pos.y, pos.z);
		SetVelocity(Vector3(velocity.x, velocity.y, velocity.z));

		// Check for player proximity to start positioning
		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
		if (!planes.empty()) {
			float dist_to_player = glm::distance(pos, planes[0]->GetPosition().Toglm());
			if (dist_to_player < 300.0f) {
				state_ = State::POSITIONING;
				state_timer_ = 0.0f;
			}
		}
	}

	void ChargerEnemy::UpdatePositioning(const EntityHandler& handler, float delta_time) {
		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
		if (planes.empty()) {
			state_ = State::ROAMING;
			return;
		}

		auto player = planes[0];
		glm::vec3 player_pos = player->GetPosition().Toglm();
		glm::vec3 player_fwd = player->GetOrientation() * glm::vec3(0, 0, -1);

		target_pos_ = player_pos + player_fwd * 40.0f;

		auto pos = GetPosition().Toglm();
		glm::vec3 to_target = target_pos_ - pos;
		float dist = glm::length(to_target);

		if (dist < 5.0f || state_timer_ > 3.0f) {
			state_ = State::PREPARING;
			state_timer_ = 0.0f;
			SetVelocity(Vector3(0, 0, 0));
			return;
		}

		glm::vec3 vel = glm::normalize(to_target) * 40.0f;
		SetVelocity(Vector3(vel.x, vel.y, vel.z));

		// Ensure we stay above ground during positioning
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(pos.x, pos.z);
		if (pos.y < h + 2.0f) pos.y = h + 2.0f;
		SetPosition(pos.x, pos.y, pos.z);

		state_timer_ += delta_time;
	}

	void ChargerEnemy::UpdatePreparing(const EntityHandler& handler, float delta_time) {
		state_timer_ += delta_time;

		// Vibration
		std::uniform_real_distribution<float> dist(-0.2f, 0.2f);
		visual_offset_ = glm::vec3(dist(gen_), dist(gen_), dist(gen_));

		// Intense red color
		float pulse = (sin(state_timer_ * 20.0f) + 1.0f) * 0.5f;
		SetColor(1.0f + pulse * 9.0f, 0.1f, 0.1f);

		if (state_timer_ > 1.5f) {
			state_ = State::CHARGING;
			state_timer_ = 0.0f;
			SetTrailRocket(true);

			auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
			if (!planes.empty()) {
				target_pos_ = planes[0]->GetPosition().Toglm();
			}
		}
	}

	void ChargerEnemy::UpdateCharging(const EntityHandler& handler, float delta_time) {
		state_timer_ += delta_time;
		auto pos = GetPosition().Toglm();

		glm::vec3 to_target = target_pos_ - pos;
		glm::vec3 dir = glm::normalize(to_target);
		glm::vec3 vel = dir * 180.0f;
		SetVelocity(Vector3(vel.x, vel.y, vel.z));

		// Collision with player
		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
		if (!planes.empty()) {
			auto player = planes[0];
			if (glm::distance(pos, player->GetPosition().Toglm()) < 10.0f) {
				player->OnHit(handler, 30.0f);
				// Continue through player, don't stop immediately
			}
		}

		if (state_timer_ > 2.0f) {
			state_ = State::ROAMING;
			SetTrailRocket(false);
			SetColor(1.0f, 1.0f, 1.0f);
		}
	}

	void ChargerEnemy::UpdateTumbling(const EntityHandler& handler, float delta_time) {
		state_timer_ += delta_time;
		if (state_timer_ > 3.0f) {
			state_ = State::ROAMING;
			SetColor(1.0f, 1.0f, 1.0f);
			rigid_body_.SetAngularVelocity(glm::vec3(0));
		}
	}

	void ChargerEnemy::UpdateDying(const EntityHandler& handler, float delta_time) {
		dissolve_sweep_ -= delta_time * 0.5f;
		if (auto model = std::dynamic_pointer_cast<Model>(shape_)) {
			model->SetDissolveSweep(glm::vec3(0, 1, 0), dissolve_sweep_);
		}

		if (dissolve_sweep_ <= 0.0f) {
			handler.QueueRemoveEntity(id_);
		}
	}

	void ChargerEnemy::OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) {
		if (state_ == State::DYING) return;

		health_ -= damage;

		// Apply tumble
		glm::vec3 force = glm::normalize(GetPosition().Toglm() - hit_point) * damage * 10.0f;
		AddForceAtPoint(force, hit_point);

		if (health_ <= 0) {
			state_ = State::DYING;
			SetVelocity(Vector3(0, 0, 0));
			SetTrailRocket(false);

			auto pos = GetPosition().Toglm();
			auto vis = handler.vis;
			handler.EnqueueVisualizerAction([pos, vis]() {
				vis->AddFireEffect(pos, FireEffectStyle::Cinder, glm::vec3(0, 1, 0), glm::vec3(0, 2, 0), 200, 1.5f);
				vis->CreateExplosion(pos, 1.0f);
			});

			if (auto* pp_handler = dynamic_cast<const KittywumpusHandler*>(&handler)) {
				pp_handler->AddScore(600, "Charger Neutralized");
			}
		} else if (state_ != State::CHARGING) {
			state_ = State::TUMBLING;
			state_timer_ = 0.0f;
			SetColor(1.0f, 0.5f, 0.5f); // Turn pinkish when hit
		}
	}

} // namespace Boidsish
