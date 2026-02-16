#include "Tracer.h"

#include "graphics.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

	Tracer::Tracer(Vector3 pos, glm::quat orientation, glm::vec3 velocity, glm::vec3 color):
		Entity<Line>(
			pos.Toglm(),
			pos.Toglm() + glm::normalize(velocity) * 2.0f,
			0.15f,
			color.r,
			color.g,
			color.b,
			1.0f
		),
		velocity_(velocity) {
		shape_->SetStyle(Line::Style::LASER);
		SetPosition(pos);
		rigid_body_.SetLinearVelocity(velocity);
		rigid_body_.SetOrientation(orientation);
		rigid_body_.linear_friction_ = 0.0f;
		rigid_body_.angular_friction_ = 0.0f;
	}

	void Tracer::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		if (lived_ > lifetime_) {
			handler.QueueRemoveEntity(GetId());
			return;
		}

		// RigidBody handles position update in EntityHandler::operator()
		// But we need to update the line endpoints to maintain the streak
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 dir = glm::normalize(velocity_);
		float     streak_length = 3.0f;

		shape_->SetStart(current_pos);
		shape_->SetEnd(current_pos - dir * streak_length);

		// Terrain collision check
		auto [height, terrain_norm] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		if (current_pos.y <= height) {
			handler.QueueRemoveEntity(GetId());
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
