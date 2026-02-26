#pragma once

#include "graph.h"
#include <glm/glm.hpp>
#include <array>

namespace Boidsish {

	class WalkingCreature : public Graph {
	public:
		WalkingCreature(int id, float x, float y, float z, float length = 4.0f);

		void Update(float delta_time) override;

		void SetTarget(const glm::vec3& target) { target_pos_ = target; }
		void SetCameraPosition(const glm::vec3& camera_pos) { camera_pos_ = camera_pos; }

	private:
		struct Leg {
			int       node_idx;
			glm::vec3 rest_offset;     // local to body
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
		int                body_node_idx_;
		int                head_node_idx_;

		glm::vec3 current_pos_;
		float     current_yaw_ = 0.0f;

		glm::vec3 target_pos_;
		glm::vec3 camera_pos_;

		int                      current_sequence_idx_ = 0;
		const std::array<int, 4> sequence_ = {0, 2, 1, 3}; // FL(0), BR(2), FR(1), BL(3)
		bool                     is_walking_ = false;
		float                    walk_phi_ = 0.0f;

		float     look_timer_ = 0.0f;
		glm::vec3 look_target_pos_;
		bool      is_looking_at_camera_ = true;

		void UpdateMovement(float delta_time);
		void UpdateNodes();
	};

} // namespace Boidsish
