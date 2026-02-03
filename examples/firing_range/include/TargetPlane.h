#pragma once
#include "PaperPlane.h"

namespace Boidsish {
	class TargetPlane : public PaperPlane {
	public:
		TargetPlane(int id = 0, Vector3 pos = Vector3(0, 4, 0));
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		glm::vec3 center_;
		float start_time_ = -1.0f;
		float radius_ = 150.0f;
		float angular_speed_ = 0.3f;
	};
}
