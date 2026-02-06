#pragma once

#include "entity.h"
#include "line.h"

namespace Boidsish {

	class Bullet: public Entity<Line> {
	public:
		Bullet(int id, Vector3 pos, glm::quat orientation, Vector3 vel, bool hostile = false);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void Explode(const EntityHandler& handler);

	private:
		bool                   hostile_;
		float                  lived_ = 0.0f;
		static constexpr float kLifetime = 3.0f;
		static constexpr float kSpeed = 400.0f;
		static constexpr float kHitRadius = 15.0f;
	};

} // namespace Boidsish
