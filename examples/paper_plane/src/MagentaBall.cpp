#include "MagentaBall.h"

#include "PaperPlane.h"
#include "graphics.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

	MagentaBall::MagentaBall(int id, Vector3 pos, Vector3 vel): Entity<Dot>(id) {
		SetPosition(pos);
		SetVelocity(vel);
		SetColor(1.0f, 0.0f, 1.0f, 1.0f); // Magenta
		SetSize(8.0f);
		SetTrailLength(30);
		SetTrailPBR(true);
		SetTrailRoughness(0.1f);
		SetTrailMetallic(0.8f);

		shape_->SetInstanced(true);

		rigid_body_.linear_friction_ = 0.0f;
		rigid_body_.angular_friction_ = 0.0f;

		UpdateShape();
	}

	void MagentaBall::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		if (lived_ > lifetime_) {
			handler.QueueRemoveEntity(GetId());
			return;
		}

		// Ballistic physics: apply gravity
		glm::vec3 vel = GetVelocity().Toglm();
		vel.y -= 9.8f * delta_time;
		SetVelocity(Vector3(vel.x, vel.y, vel.z));

		// Collision logic
		glm::vec3 my_pos = GetPosition().Toglm();

		// Ground collision
		auto [h, norm] = handler.vis->GetTerrain()->GetPointProperties(my_pos.x, my_pos.z);
		// Prevent immediate explosion on launch
		if (lived_ > 0.5f) {
			has_cleared_ground_ = true;
		}

		if (has_cleared_ground_ && my_pos.y < h) {
			// Trigger a small explosion or effect on ground hit
			handler.EnqueueVisualizerAction([my_pos, &handler]() {
				handler.vis->AddFireEffect(my_pos, FireEffectStyle::Explosion, glm::vec3(0, 1, 0), glm::vec3(0), -1, 0.5f);
			});
			handler.QueueRemoveEntity(GetId());
			return;
		}

		// Player collision
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (!planes.empty()) {
			PaperPlane* plane = planes[0];
			float dist = glm::distance(my_pos, plane->GetPosition().Toglm());
			if (dist < 5.0f) {
				plane->TriggerDamage();
				handler.EnqueueVisualizerAction([my_pos, &handler]() {
					handler.vis->AddFireEffect(my_pos, FireEffectStyle::Explosion, glm::vec3(0, 1, 0), glm::vec3(0), -1, 1.0f);
				});
				handler.QueueRemoveEntity(GetId());
				return;
			}
		}

		UpdateShape();
	}

} // namespace Boidsish
