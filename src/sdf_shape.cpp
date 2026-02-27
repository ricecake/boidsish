#include "sdf_shape.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	SdfShape::SdfShape(
		int id,
		float x,
		float y,
		float z,
		float radius,
		float r,
		float g,
		float b,
		float a,
		float smoothness,
		float charge,
		int type
	) :
		Shape(id, x, y, z, r, g, b, a),
		radius_(radius),
		smoothness_(smoothness),
		charge_(charge),
		type_(type) {}

	glm::mat4 SdfShape::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale() * radius_);
		return model;
	}

	void SdfShape::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		(void)out_packets;
		(void)context;
		// SdfShape does not generate render packets as it is rendered by a post-processing effect
	}

	float SdfShape::GetBoundingRadius() const {
		// Include smoothness in bounding radius as it affects the potential influence area
		return (radius_ + smoothness_) * glm::length(GetScale());
	}

	AABB SdfShape::GetAABB() const {
		glm::vec3 center(GetX(), GetY(), GetZ());
		float r = GetBoundingRadius();
		return AABB(center - glm::vec3(r), center + glm::vec3(r));
	}

} // namespace Boidsish
