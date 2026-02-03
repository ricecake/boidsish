#include "TargetPlane.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {
	TargetPlane::TargetPlane(int id, Vector3 pos) : PaperPlane(id, pos) {
		center_ = pos.Toglm();
		// Initial position will be at angle 0
		SetPosition(center_.x + radius_, center_.y, center_.z);
		UpdateShape();
	}

	void TargetPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (start_time_ < 0) start_time_ = time;
		float elapsed = time - start_time_;

		float angle = elapsed * angular_speed_;

		glm::vec3 offset = glm::vec3(cos(angle) * radius_, 0, sin(angle) * radius_);
		glm::vec3 new_pos = center_ + offset;
		SetPosition(new_pos.x, new_pos.y, new_pos.z);

		// Direction is tangent to the circle: (-sin(angle), 0, cos(angle))
		glm::vec3 tangent = glm::normalize(glm::vec3(-sin(angle), 0, cos(angle)));

		// Use a simple look-at to orient the plane along the tangent
		// Note: glm::quatLookAt might have different conventions, let's use a manual approach if needed
		// but typically quatLookAt(direction, up) works if direction is -z
		// Actually, PaperPlane expects -z to be forward.

		orientation_ = glm::quatLookAt(tangent, glm::vec3(0, 1, 0));

		// Add some bank (roll) because it's turning
		glm::quat bank = glm::angleAxis(glm::radians(-30.0f), glm::vec3(0, 0, 1));
		orientation_ = orientation_ * bank;

		UpdateShape();
	}
}
