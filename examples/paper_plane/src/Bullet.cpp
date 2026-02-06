#include "Bullet.h"

#include "FighterPlane.h"
#include "PaperPlane.h"
#include "graphics.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

	Bullet::Bullet(int id, Vector3 pos, glm::quat orientation, Vector3 vel, bool hostile):
		Entity<Line>(
			id,
			pos.Toglm(),
			pos.Toglm() + orientation * glm::vec3(0, 0, -5.0f),
			hostile ? 2.0f : 1.5f
		),
		hostile_(hostile) {

		SetPosition(pos);
		glm::vec3 fwd = orientation * glm::vec3(0, 0, -1);
		SetVelocity(vel.Toglm() + fwd * kSpeed);

		if (hostile_) {
			SetColor(1.0f, 0.2f, 0.2f, 1.0f);
		} else {
			SetColor(1.0f, 1.0f, 0.3f, 1.0f);
		}

		UpdateShape();
	}

	void Bullet::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		lived_ += delta_time;
		if (lived_ > kLifetime) {
			handler.QueueRemoveEntity(id_);
			return;
		}

		auto pos = GetPosition().Toglm();
		auto vel = GetVelocity().Toglm();

		if (glm::length(vel) > 0.1f) {
			shape_->SetEnd(pos + glm::normalize(vel) * 5.0f);
		}

		if (hostile_) {
			// Check hit against player
			auto planes = handler.GetEntitiesByType<PaperPlane>();
			if (!planes.empty()) {
				auto player = planes[0];
				if (glm::distance(pos, player->GetPosition().Toglm()) < kHitRadius) {
					player->TriggerDamage();
					Explode(handler);
					return;
				}
			}
		} else {
			// Check hit against fighters
			auto fighters = handler.GetEntitiesByType<FighterPlane>();
			for (auto& fighter : fighters) {
				if (fighter->GetState() != FighterPlane::State::CRASHING &&
				    glm::distance(pos, fighter->GetPosition().Toglm()) < kHitRadius * 2.0f) {
					fighter->ShotDown(handler);
					Explode(handler);
					return;
				}
			}
		}

		// Terrain hit
		auto [h, n] = handler.GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
		if (pos.y <= h) {
			Explode(handler);
		}
	}

	void Bullet::Explode(const EntityHandler& handler) {
		handler.QueueRemoveEntity(id_);
	}

} // namespace Boidsish
