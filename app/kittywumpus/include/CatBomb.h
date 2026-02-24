#pragma once

#include "entity.h"
#include "model.h"
#include <glm/vec3.hpp>

namespace Boidsish {

	class CatBomb: public Entity<Model> {
	public:
		CatBomb(int id = 0, Vector3 pos = {0, 0, 0}, glm::vec3 dir = {0, 0, 0}, Vector3 vel = {0, 0, 0});

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		void Explode(const EntityHandler& handler);

		// Constants
		static constexpr float kGravity = 0.50f;
		static constexpr float kExplosionDisplayTime = 2.0f;

		// State
		float                        lived_ = 0.0f;
		bool                         exploded_ = false;
		std::shared_ptr<SoundEffect> explode_sound_ = nullptr;
	};

} // namespace Boidsish
