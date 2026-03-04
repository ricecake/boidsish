#include "RedDotEnemy.h"

#include <random>
#include "KittywumpusPlane.h"
#include "Tracer.h"
#include "graphics.h"
#include "KittywumpusHandler.h"
#include "fire_effect.h"

namespace Boidsish {

	RedDotEnemy::RedDotEnemy(int id, Vector3 pos) : Entity<Dot>(id, 100.0f, 1.0f, 0.0f, 0.0f, 1.0f) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(0.5f)); // Make it a visible dot
		SetColor(1.0f, 0.0f, 0.0f); // Red
		PickHoverOffset();
	}

	void RedDotEnemy::PickHoverOffset() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
		hover_offset_ = glm::vec3(dist(gen), dist(gen), dist(gen));
	}

	void RedDotEnemy::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (state_ == State::DEAD) return;

		if (state_ == State::DYING) {
			death_timer_ += delta_time;
			float t = glm::clamp(death_timer_ / kDeathDuration, 0.0f, 1.0f);

			// Turn black
			SetColor(1.0f - t, 0.0f, 0.0f);

			if (death_timer_ >= kDeathDuration) {
				auto pos = GetPosition().Toglm();
				auto vis = handler.vis;
				handler.EnqueueVisualizerAction([pos, vis]() {
					vis->AddFireEffect(
						pos,
						FireEffectStyle::Cinder,
						glm::vec3(0, 1, 0),
						glm::vec3(0, 2, 0),
						500,
						1.5f
					);
				});
				state_ = State::DEAD;
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		// Floating behavior
		hover_timer_ += delta_time;
		if (hover_timer_ > 2.0f) {
			PickHoverOffset();
			hover_timer_ = 0.0f;
		}

		auto planes = handler.GetEntitiesByType<KittywumpusPlane>();
		if (planes.empty()) return;
		auto player = planes[0];
		glm::vec3 player_pos = player->GetPosition().Toglm();

		// Move towards a point near player but keep some distance
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 to_player = player_pos - current_pos;
		float dist_to_player = glm::length(to_player);

		glm::vec3 target_pos = player_pos - glm::normalize(to_player) * 20.0f + hover_offset_;
		glm::vec3 move_dir = target_pos - current_pos;
		if (glm::length(move_dir) > 0.1f) {
			glm::vec3 vel = glm::normalize(move_dir) * 5.0f;
			SetVelocity(Vector3(vel.x, vel.y, vel.z));
		}

		// Fire bullets occasionally when in front of the player
		glm::vec3 player_fwd = player->GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 player_to_me = current_pos - player_pos;
		if (glm::length(player_to_me) > 0.001f) {
			float dot = glm::dot(player_fwd, glm::normalize(player_to_me));
			if (dot > 0.5f) { // In front of player
				fire_timer_ += delta_time;
				if (fire_timer_ > 2.0f) {
					fire_timer_ = 0.0f;
					glm::vec3 fire_dir = glm::normalize(-player_to_me);
					handler.QueueAddEntity<Tracer>(
						current_pos + fire_dir * 2.0f,
						glm::quat(1,0,0,0), // dummy
						fire_dir * 100.0f,
						glm::vec3(1.0f, 0.2f, 0.2f),
						id_
					);
				}
			}
		}
	}

	void RedDotEnemy::OnHit(const EntityHandler& handler, float damage) {
		if (state_ != State::ALIVE) return;

		health_ -= damage;
		if (health_ <= 0) {
			state_ = State::DYING;
			death_timer_ = 0.0f;
			SetVelocity(Vector3(0, 0, 0));
			if (auto* pp_handler = dynamic_cast<const KittywumpusHandler*>(&handler)) {
				pp_handler->AddScore(500, "Red Dot Neutralized");
			}
		}
	}

} // namespace Boidsish
