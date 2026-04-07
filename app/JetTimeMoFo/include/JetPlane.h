#pragma once

#include <memory>
#include "entity.h"
#include "model.h"
#include "JetInputController.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	class JetPlane : public Entity<Model> {
	public:
		JetPlane(int id = 0);

		void SetController(std::shared_ptr<JetInputController> controller);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void UpdateShape() override;

		float GetThrottle() const { return throttle_; }
		bool IsStabilizationEnabled() const { return stabilization_enabled_; }

	private:
		std::shared_ptr<JetInputController> controller_;
		glm::quat orientation_;
		glm::vec3 rotational_velocity_;
		float forward_speed_;
		float throttle_; // 0.0 to 1.0
		bool stabilization_enabled_ = true;
	};

} // namespace Boidsish
