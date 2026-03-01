#pragma once

#include <array>
#include <string>

#include "light.h"
#include "model.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class WalkingCreature: public Model {
	public:
		WalkingCreature(int id, float x, float y, float z, float length = 4.0f);

		void Update(float delta_time) override;

		void SetTarget(const glm::vec3& target) { target_pos_ = target; }

		void SetCameraPosition(const glm::vec3& camera_pos) { camera_pos_ = camera_pos; }

		const Light& GetSpotlight() const { return spotlight_; }

	private:
		struct Leg {
			std::string bone_name;
			std::string thigh_bone_name;
			std::string knee_bone_name;
			std::string foot_bone_name;

			glm::vec3 rest_offset; // local to body
			glm::vec3 world_foot_pos;
			glm::vec3 step_start_pos;
			glm::vec3 step_target_pos;
			float     progress = 1.0f; // 1.0 = planted
			bool      is_moving = false;
		};

		float length_;
		float width_;
		float height_;
		float step_duration_ = 0.5f;

		std::array<Leg, 4> legs_;
		glm::vec3          body_offset_ = glm::vec3(0.0f);

		glm::vec3 current_pos_;
		float     current_yaw_ = 0.0f;

		glm::vec3 target_pos_;
		glm::vec3 camera_pos_;

		Light spotlight_;

		bool  is_walking_ = false;

		void UpdateMovement(float delta_time);
		void UpdateSkeleton(float delta_time);
		void UpdateBalance();
	};

} // namespace Boidsish
