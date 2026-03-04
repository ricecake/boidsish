#pragma once

#include "model.h"
#include "procedural_ir.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Boidsish {

	class ProceduralWalkingCreature : public Shape {
	public:
		ProceduralWalkingCreature(int id, float x, float y, float z, float length = 4.0f);

		void Update(float delta_time);

		void PrepareResources(Megabuffer* megabuffer = nullptr) const override;
		void render() const override;
		void render(Shader& shader, const glm::mat4& model_matrix) const override;
		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;
		glm::mat4 GetModelMatrix() const override;
		AABB GetAABB() const override;
		std::string GetInstanceKey() const override { return "ProceduralWalkingCreature:" + std::to_string(GetId()); }

		void SetTarget(const glm::vec3& target) { target_pos_ = target; }

	private:
		struct Leg {
			std::string name;
			std::string effector_name;
			glm::vec3 rest_offset; // local to body
			glm::vec3 world_foot_pos;
			glm::vec3 step_start_pos;
			glm::vec3 step_target_pos;
			float progress = 1.0f; // 1.0 = planted
			bool is_moving = false;
		};

		float length_;
		float width_;
		float height_;
		float step_duration_ = 0.4f;

		std::vector<Leg> legs_;
		std::shared_ptr<Model> model_;

		glm::vec3 current_pos_;
		float current_yaw_ = 0.0f;
		glm::vec3 target_pos_;

		int current_sequence_idx_ = 0;
		std::vector<int> sequence_ = {0, 2, 1, 3}; // FL, BR, FR, BL
		float walk_phi_ = 0.0f;
		bool is_walking_ = false;

		ProceduralIR GenerateIR();
		void UpdateMovement(float delta_time);
	};

} // namespace Boidsish
