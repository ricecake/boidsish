#include "RefractiveStalker.h"
#include "KittywumpusPlane.h"
#include "Beam.h"
#include "KittywumpusHandler.h"
#include "fire_effect.h"
#include "graphics.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	RefractiveStalker::RefractiveStalker(int id, const glm::vec3& pos) : Entity<WalkingCreature>(id, pos.x, pos.y, pos.z, 20.0f) {
		SetPosition(Vector3(pos.x, pos.y, pos.z));
		base_height_ = 20.0f * 0.5f; // From WalkingCreature logic height = length * 0.5

		// Initial stalking state visuals
		shape_->SetColor(0.8f, 0.9f, 1.0f, 0.1f);
		shape_->SetRefractive(true, 1.05f);
		shape_->SetUsePBR(true);
		shape_->SetRoughness(0.1f);
		shape_->SetMetallic(0.1f);
	}

	void RefractiveStalker::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (state_ == State::DYING) return;

		// Find player
		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
		if (planes.empty()) return;
		auto player = planes[0];
		glm::vec3 player_pos = player->GetPosition().Toglm();
		glm::vec3 my_pos = GetPosition().Toglm();

		// Handle Beam spawn
		if (beam_id_ == -1) {
			beam_id_ = handler.GetNextId();
			handler.QueueAddEntityWithId<Beam>(beam_id_, id_);
		}

		switch (state_) {
			case State::STALKING:
				UpdateStalking(handler, delta_time);
				break;
			case State::ATTACKING:
				UpdateAttacking(handler, delta_time);
				break;
			case State::COOLDOWN:
				UpdateCooldown(handler, delta_time);
				break;
			default:
				break;
		}

		// Ensure height is clamped to terrain if needed (WalkingCreature does its own node updates)
		// but we want the base position to follow terrain.
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(my_pos.x, my_pos.z);
		SetPosition(my_pos.x, h, my_pos.z);

		// Update WalkingCreature specific parameters
		shape_->SetCameraPosition(handler.vis->GetCamera().pos());
	}

	void RefractiveStalker::UpdateStalking(const EntityHandler& handler, float delta_time) {
		auto player = handler.GetEntitiesByType<KittywumpusPlane>()[0];
		glm::vec3 player_pos = player->GetPosition().Toglm();
		glm::vec3 my_pos = GetPosition().Toglm();

		glm::vec3 to_player = player_pos - my_pos;
		float dist = glm::length(to_player);

		if (dist < attack_dist_) {
			state_ = State::ATTACKING;
			state_timer_ = 0.0f;
			shape_->SetTarget(my_pos + glm::normalize(to_player) * 0.1f); // Stop moving
			return;
		}

		// Move towards player
		glm::vec3 dir = glm::normalize(to_player);
		glm::vec3 move_pos = my_pos + dir * stalk_speed_ * delta_time;
		shape_->SetTarget(move_pos + dir * 5.0f);

		// Visuals
		shape_->SetColor(0.8f, 0.9f, 1.0f, 0.1f);
		shape_->SetRefractive(true, 1.05f);
		shape_->SetHeight(base_height_);
		shape_->SetStepDuration(0.3f); // Fast steps

		if (beam_id_ != -1) {
			if (auto beam = std::dynamic_pointer_cast<Beam>(handler.GetEntity(beam_id_))) {
				beam->SetSelected(false);
				beam->SetRequesting(false);
			}
		}
	}

	void RefractiveStalker::UpdateAttacking(const EntityHandler& handler, float delta_time) {
		state_timer_ += delta_time;

		auto player = handler.GetEntitiesByType<KittywumpusPlane>()[0];
		glm::vec3 player_pos = player->GetPosition().Toglm();
		glm::vec3 my_pos = GetPosition().Toglm();
		glm::vec3 to_player = player_pos - my_pos;

		// Face player
		shape_->SetTarget(player_pos);

		// Visuals: Crouching and becoming visible
		float transition = std::min(1.0f, state_timer_ / 1.0f);
		shape_->SetColor(
			glm::mix(0.8f, 1.0f, transition),
			glm::mix(0.9f, 0.2f, transition),
			glm::mix(1.0f, 0.2f, transition),
			glm::mix(0.1f, 0.8f, transition)
		);
		shape_->SetRefractive(true, glm::mix(1.05f, 1.6f, transition));
		shape_->SetHeight(glm::mix(base_height_, base_height_ * 0.4f, transition));

		if (beam_id_ != -1) {
			if (auto beam = std::dynamic_pointer_cast<Beam>(handler.GetEntity(beam_id_))) {
				beam->SetSelected(true);
				beam->SetOffset(glm::vec3(0, base_height_ * 0.8f, 10.0f)); // Approximate head position
				glm::vec3 rel_dir = player_pos - (my_pos + glm::vec3(0, base_height_ * 0.8f, 0));
				beam->SetRelativeDirection(glm::normalize(rel_dir));

				if (state_timer_ > 1.5f) {
					beam->SetRequesting(true);
				}

				if (beam->GetState() == Beam::State::COOLDOWN) {
					state_ = State::COOLDOWN;
					state_timer_ = 0.0f;
					beam->SetRequesting(false);
					beam->SetSelected(false);
				}
			}
		}

		// If player gets too far away or beam takes too long
		if (state_timer_ > 5.0f) {
			state_ = State::COOLDOWN;
			state_timer_ = 0.0f;
		}
	}

	void RefractiveStalker::UpdateCooldown(const EntityHandler& handler, float delta_time) {
		state_timer_ += delta_time;

		// Visuals: returning to stalking state
		float transition = std::min(1.0f, state_timer_ / 2.0f);
		shape_->SetColor(
			glm::mix(1.0f, 0.8f, transition),
			glm::mix(0.2f, 0.9f, transition),
			glm::mix(0.2f, 1.0f, transition),
			glm::mix(0.8f, 0.1f, transition)
		);
		shape_->SetRefractive(true, glm::mix(1.6f, 1.05f, transition));
		shape_->SetHeight(glm::mix(base_height_ * 0.4f, base_height_, transition));

		if (state_timer_ > 3.0f) {
			state_ = State::STALKING;
			state_timer_ = 0.0f;
		}
	}

	void RefractiveStalker::OnHit(const EntityHandler& handler, float damage) {
		if (state_ == State::DYING) return;

		health_ -= damage;
		if (health_ <= 0) {
			state_ = State::DYING;
			auto pos = GetPosition().Toglm();
			auto vis = handler.vis;
			handler.EnqueueVisualizerAction([pos, vis]() {
				vis->AddFireEffect(
					pos,
					FireEffectStyle::Cinder,
					glm::vec3(0, 1, 0),
					glm::vec3(0, 5, 0),
					1000,
					2.5f
				);
			});
			if (beam_id_ != -1) {
				const_cast<EntityHandler&>(handler).QueueRemoveEntity(beam_id_);
			}
			const_cast<EntityHandler&>(handler).QueueRemoveEntity(id_);

			if (auto* kh = dynamic_cast<const KittywumpusHandler*>(&handler)) {
				kh->AddScore(2500, "Refractive Stalker Terminated");
			}
		}
	}

} // namespace Boidsish
