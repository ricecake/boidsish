#pragma once

#include "entity.h"
#include "model.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Boidsish {

	class WalkingTank : public Entity<Model> {
	public:
		WalkingTank(int id, float x, float y, float z);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

		void SetTarget(const glm::vec3& target) { target_pos_ = target; }

	private:
		struct Leg {
			std::string name;
			std::string effector;
			glm::vec3   rest_offset; // Local to body
			glm::vec3   world_foot_pos;
			glm::vec3   step_start_pos;
			glm::vec3   step_target_pos;
			float       progress = 1.0f;
			bool        is_moving = false;
		};

		std::vector<Leg> legs_;
		float            walk_phi_ = 0.0f;
		float            step_duration_ = 0.6f;
		float            step_height_ = 0.4f;
		float            stride_length_ = 1.2f;

		glm::vec3 target_pos_;
		float current_yaw_ = 0.0f;

		void InitializeRig();
		void UpdateMovement(const EntityHandler& handler, float delta_time);
	};

} // namespace Boidsish
