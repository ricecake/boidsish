#pragma once

#include "entity.h"
#include "line.h"

namespace Boidsish {

	class Tracer: public Entity<Line> {
	public:
		Tracer(
			int       id = 0,
			Vector3   pos = {0, 0, 0},
			glm::quat orientation = {1, 0, 0, 0},
			glm::vec3 velocity = {0, 0, 0},
			glm::vec3 color = {1, 1, 1},
			int       owner_id = -1
		);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		glm::vec3              velocity_;
		int                    owner_id_;
		float                  lived_ = 0.0f;
		static constexpr float lifetime_ = 1.2f;
	};

} // namespace Boidsish
