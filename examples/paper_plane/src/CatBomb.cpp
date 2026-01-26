#include "CatBomb.h"

#include "fire_effect.h"
#include "graphics.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	CatBomb::CatBomb(int id, Vector3 pos, glm::vec3 dir, Vector3 vel):
		Entity<Model>(id, "assets/bomb_shading_v005.obj", true) {
		rigid_body_.linear_friction_ = 0.01f;
		rigid_body_.angular_friction_ = 0.01f;

		SetOrientToVelocity(true);
		SetPosition(pos.x, pos.y, pos.z);
		auto netVelocity = glm::vec3(vel.x, vel.y, vel.z) + 0.5f * glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
		SetVelocity(netVelocity.x, netVelocity.y, netVelocity.z);

		SetTrailLength(50);
		shape_->SetScale(glm::vec3(0.01f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
		);
		shape_->SetInstanced(true);
	}

	void CatBomb::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		auto pos = GetPosition();
		lived_ += delta_time;

		if (exploded_) {
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		auto [height, norm] = handler.vis->GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler);
			return;
		}

		auto velo = GetVelocity();
		velo += Vector3(0, -kGravity, 0);
		SetVelocity(velo);
	}

	void CatBomb::Explode(const EntityHandler& handler) {
		if (exploded_)
			return;

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->CreateExplosion(glm::vec3(pos.x, pos.y, pos.z), 2.5f);
			// handler.vis->AddFireEffect(
			// 	glm::vec3(pos.x, pos.y, pos.z),
			// 	FireEffectStyle::Explosion,
			// 	glm::vec3(0, 1, 0),
			// 	glm::vec3(0, 0, 0),
			// 	-1,
			// 	2.0f
			// );
		});

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));
		SetTrailLength(0);
		explode_sound_ = handler.vis
							 ->AddSoundEffect("assets/rocket_explosion.wav", pos.Toglm(), GetVelocity().Toglm(), 25.0f);
	}

} // namespace Boidsish
