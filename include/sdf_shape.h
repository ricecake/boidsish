#pragma once

#include "shape.h"

namespace Boidsish {

	class SdfShape : public Shape {
	public:
		SdfShape(
			int id = 0,
			float x = 0.0f,
			float y = 0.0f,
			float z = 0.0f,
			float radius = 5.0f,
			float r = 1.0f,
			float g = 1.0f,
			float b = 1.0f,
			float a = 1.0f,
			float smoothness = 2.0f,
			float charge = 1.0f,
			int type = 0
		);

		// Size accessor/mutator for Dot compatibility
		inline float GetSize() const { return radius_; }
		inline void SetSize(float size) { radius_ = size; MarkDirty(); }

		inline float GetRadius() const { return radius_; }
		inline void SetRadius(float radius) { radius_ = radius; MarkDirty(); }

		inline float GetSmoothness() const { return smoothness_; }
		inline void SetSmoothness(float smoothness) { smoothness_ = smoothness; MarkDirty(); }

		inline float GetCharge() const { return charge_; }
		inline void SetCharge(float charge) { charge_ = charge; MarkDirty(); }

		inline int GetSdfType() const { return type_; }
		inline void SetSdfType(int type) { type_ = type; MarkDirty(); }

		// Shape overrides
		void render() const override {}
		void render(Shader& shader, const glm::mat4& model_matrix) const override { (void)shader; (void)model_matrix; }
		glm::mat4 GetModelMatrix() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;

		float GetBoundingRadius() const override;
		AABB GetAABB() const override;

		std::string GetInstanceKey() const override { return "SdfShape"; }
		bool IsSdf() const override { return true; }

	private:
		float radius_;
		float smoothness_;
		float charge_;
		int type_;
	};

} // namespace Boidsish
