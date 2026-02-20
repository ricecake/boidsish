#include "Tracer.h"

#include "graphics.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

	Tracer::Tracer(int id, Vector3 pos, glm::quat orientation, glm::vec3 velocity, glm::vec3 color, int owner_id):
		Entity<Line>(
			id,
			id,
			pos.Toglm(),
			pos.Toglm() + glm::normalize(velocity) * 2.0f,
			0.15f,
			color.r,
			color.g,
			color.b,
			1.0f
		),
		velocity_(velocity),
		owner_id_(owner_id) {
		shape_->SetStyle(Line::Style::LASER);
		SetPosition(pos);
		SetTrailLength(0); // Remove long lingering trails
		rigid_body_.SetLinearVelocity(velocity);
		rigid_body_.SetOrientation(orientation);
		rigid_body_.linear_friction_ = 0.0f;
		rigid_body_.angular_friction_ = 0.0f;
	}

	void Tracer::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		if (lived_ > lifetime_) {
			handler.QueueRemoveEntity(id_);
			return;
		}

		// RigidBody handles position update in EntityHandler::operator()
		// But we need to update the line endpoints to maintain the streak
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 dir = glm::normalize(velocity_);
		float     streak_length = 3.0f;

		shape_->SetStart(current_pos);
		shape_->SetEnd(current_pos - dir * streak_length);

		// Entity hit detection
		auto spatial_handler = dynamic_cast<const SpatialEntityHandler*>(&handler);
		if (spatial_handler) {
			auto targets = spatial_handler->GetEntitiesInRadius<EntityBase>(GetPosition(), 8.0f); // Forgiving radius
			for (auto& target : targets) {
				if (target->GetId() == owner_id_ || target->GetId() == id_)
					continue;

				// Skip other tracers to avoid bullet-bullet collisions
				if (dynamic_cast<Tracer*>(target.get()))
					continue;

				target->OnHit(handler, 10.0f);
				handler.QueueRemoveEntity(id_);

				// Small hit effect
				auto vis = handler.vis;
				handler.EnqueueVisualizerAction([=]() {
					if (vis) {
						vis->AddFireEffect(current_pos, FireEffectStyle::Sparks, -dir, glm::vec3(0), 5, 0.2f);
					}
				});
				return;
			}
		}

		// Terrain collision check
		auto [height, terrain_norm] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		if (current_pos.y <= height) {
			handler.QueueRemoveEntity(id_);
			// Small impact effect
			auto      vis = handler.vis;
			glm::vec3 spark_norm = terrain_norm;
			handler.EnqueueVisualizerAction([=]() {
				if (vis) {
					vis->AddFireEffect(current_pos, FireEffectStyle::Sparks, spark_norm, glm::vec3(0), 5, 0.3f);
				}
			});
		}
	}

} // namespace Boidsish
