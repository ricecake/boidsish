#pragma once

#include <memory>
#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

struct PaperPlaneInputController {
	bool pitch_up = false;
	bool pitch_down = false;
	bool yaw_left = false;
	bool yaw_right = false;
	bool roll_left = false;
	bool roll_right = false;
	bool boost = false;
};

class PaperPlane: public Entity<Model> {
public:
	PaperPlane(int id = 0);

	void SetController(std::shared_ptr<PaperPlaneInputController> controller);

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	void UpdateShape() override;

private:
	std::shared_ptr<PaperPlaneInputController> controller_;
	glm::quat                                  orientation_;
	glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
	float                                      forward_speed_;
};

}
