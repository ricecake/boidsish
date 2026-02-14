#include "Blimp.h"

#include <algorithm>

#include "GuidedMissile.h"
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "graphics.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	Blimp::Blimp(int id, Vector3 pos):
		Entity<Model>(id, "assets/utah_teapot.obj", false), eng_(rd_()) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(15.0f, 8.0f, 8.0f));
		shape_->SetInstanced(true);
		SetColor(0.8f, 0.2f, 0.2f); // Red blimp
		UpdateShape();
	}

	void Blimp::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		auto targets = handler.GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto      plane = targets[0];
		glm::vec3 player_pos = plane->GetPosition().Toglm();
		glm::vec3 my_pos = GetPosition().Toglm();

		float dist = glm::distance(player_pos, my_pos);

		// 1. Repositioning logic (Catch up)
		if (dist > 2500.0f) {
			std::uniform_real_distribution<float> angle_dist(0, 2.0f * glm::pi<float>());
			float                                 angle = angle_dist(eng_);
			glm::vec3                             offset = glm::vec3(cos(angle), 0, sin(angle)) * 1000.0f;
			SetPosition(player_pos.x + offset.x, 150.0f, player_pos.z + offset.z);
			SetVelocity(0, 0, 0);
			return;
		}

		// 2. Movement logic: try to stay at ~900m distance, 150m altitude
		// We want to be at the same horizontal angle relative to the player, but at the target distance.
		glm::vec3 to_player = player_pos - my_pos;
		glm::vec3 horizontal_to_player = glm::vec3(to_player.x, 0, to_player.z);
		if (glm::length(horizontal_to_player) < 1e-4f) {
			horizontal_to_player = glm::vec3(1, 0, 0);
		}

		glm::vec3 desired_pos = player_pos - glm::normalize(horizontal_to_player) * 900.0f;
		desired_pos.y = 150.0f;

		glm::vec3 move_dir = desired_pos - my_pos;
		float     move_dist = glm::length(move_dir);
		if (move_dist > 1.0f) {
			float speed = 15.0f;
			if (move_dist > 500.0f) {
				speed = 60.0f; // Catch up faster if significantly out of position
			}
			glm::vec3 vel = glm::normalize(move_dir) * speed;
			SetVelocity(vel.x, vel.y, vel.z);
		} else {
			SetVelocity(0, 0, 0);
		}

		// 3. Firing logic
		fire_timer_ += delta_time;
		float fire_interval = 5.0f;
		if (fire_timer_ >= fire_interval) {
			fire_timer_ = 0.0f;

			// Number of missiles scales with proximity and damage
			int num_missiles = 1;
			// Closer = more missiles (up to 5 extra)
			num_missiles += static_cast<int>((1.0f - std::clamp(dist / 1500.0f, 0.0f, 1.0f)) * 5);
			// Damaged = more missiles (up to 5 extra)
			num_missiles += static_cast<int>((1.0f - health_ / max_health_) * 5);

			for (int i = 0; i < num_missiles; ++i) {
				handler.QueueAddEntity<GuidedMissile>(GetPosition());
			}
		}

		// 4. Orientation: look at player
		glm::vec3 dir = glm::normalize(player_pos - my_pos);
		SetOrientation(glm::quatLookAt(dir, glm::vec3(0, 1, 0)));

		UpdateShape();
	}

	void Blimp::OnHit(const EntityHandler& handler, float damage) {
		health_ -= damage;
		if (health_ <= 0) {
			if (auto* pp_handler = dynamic_cast<const PaperPlaneHandler*>(&handler)) {
				pp_handler->AddScore(2000, "Blimp Destroyed");
			}

			handler.EnqueueVisualizerAction([this, &handler]() {
				if (handler.vis) {
					handler.vis->TriggerComplexExplosion(
						this->shape_,
						glm::vec3(0, 1, 0),
						5.0f,
						FireEffectStyle::Explosion
					);
				}
				handler.QueueRemoveEntity(this->GetId());
			});
		}
	}

} // namespace Boidsish
