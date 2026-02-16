#include "mesh_hole_deformation.h"
#include <algorithm>
#include <limits>
#include <glm/gtx/norm.hpp>

namespace Boidsish {

	MeshHoleDeformation::MeshHoleDeformation(uint32_t id, const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indices)
		: TerrainDeformation(id) {

		min_bound_ = glm::vec3(std::numeric_limits<float>::max());
		max_bound_ = glm::vec3(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			Triangle tri;
			tri.v0 = vertices[indices[i]];
			tri.v1 = vertices[indices[i + 1]];
			tri.v2 = vertices[indices[i + 2]];

			tri.min_bound = glm::min(tri.v0, glm::min(tri.v1, tri.v2));
			tri.max_bound = glm::max(tri.v0, glm::max(tri.v1, tri.v2));

			triangles_.push_back(tri);

			min_bound_ = glm::min(min_bound_, tri.min_bound);
			max_bound_ = glm::max(max_bound_, tri.max_bound);
		}
	}

	void MeshHoleDeformation::GetBounds(glm::vec3& out_min, glm::vec3& out_max) const {
		out_min = min_bound_;
		out_max = max_bound_;
	}

	float MeshHoleDeformation::GetMaxRadius() const {
		glm::vec3 center = GetCenter();
		float r_sq = 0.0f;
		r_sq = std::max(r_sq, glm::length2(min_bound_ - center));
		r_sq = std::max(r_sq, glm::length2(max_bound_ - center));
		return std::sqrt(r_sq);
	}

	bool MeshHoleDeformation::ContainsPoint(const glm::vec3& world_pos) const {
		if (world_pos.x < min_bound_.x || world_pos.x > max_bound_.x ||
			world_pos.y < min_bound_.y || world_pos.y > max_bound_.y ||
			world_pos.z < min_bound_.z || world_pos.z > max_bound_.z) {
			return false;
		}
		return true;
	}

	bool MeshHoleDeformation::ContainsPointXZ(float x, float z) const {
		if (x < min_bound_.x || x > max_bound_.x ||
			z < min_bound_.z || z > max_bound_.z) {
			return false;
		}
		return true;
	}

	float MeshHoleDeformation::ComputeHeightDelta(float x, float z, float current_height) const {
		(void)x; (void)z; (void)current_height;
		return 0.0f; // Mesh hole doesn't change height, it just discards
	}

	bool MeshHoleDeformation::IsHole(float x, float z, float current_height) const {
		if (!ContainsPointXZ(x, z)) return false;

		// Point on the terrain surface to check
		glm::vec3 p(x, current_height, z);

		// Quick Y check
		if (p.y < min_bound_.y || p.y > max_bound_.y) return false;

		// Ray-casting along +Y axis
		glm::vec3 ray_dir(0.0f, 1.0f, 0.0f);
		int intersections = 0;

		for (const auto& tri : triangles_) {
			// Skip triangles that can't possibly intersect based on XZ
			if (x < tri.min_bound.x || x > tri.max_bound.x ||
				z < tri.min_bound.z || z > tri.max_bound.z) {
				continue;
			}

			float t;
			if (RayTriangleIntersect(p, ray_dir, tri, t)) {
				if (t >= 0.0f) {
					intersections++;
				}
			}
		}

		return (intersections % 2) != 0;
	}

	glm::vec3 MeshHoleDeformation::TransformNormal(float x, float z, const glm::vec3& original_normal) const {
		(void)x; (void)z;
		return original_normal;
	}

	DeformationResult MeshHoleDeformation::ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const {
		DeformationResult result;
		if (!ContainsPointXZ(x, z)) return result;

		if (IsHole(x, z, current_height)) {
			result.applies = true;
			result.is_hole = true;
			result.blend_weight = 1.0f;
		}

		return result;
	}

	DeformationDescriptor MeshHoleDeformation::GetDescriptor() const {
		DeformationDescriptor desc;
		desc.type_name = GetTypeName();
		desc.center = GetCenter();
		desc.dimensions = max_bound_ - min_bound_;
		desc.deformation_type = DeformationType::Subtractive;
		return desc;
	}

	bool MeshHoleDeformation::RayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir, const Triangle& tri, float& t) const {
		const float EPSILON = 0.0000001f;
		glm::vec3 edge1 = tri.v1 - tri.v0;
		glm::vec3 edge2 = tri.v2 - tri.v0;
		glm::vec3 h = glm::cross(dir, edge2);
		float a = glm::dot(edge1, h);

		if (a > -EPSILON && a < EPSILON) return false;

		float f = 1.0f / a;
		glm::vec3 s = orig - tri.v0;
		float u = f * glm::dot(s, h);

		if (u < 0.0f || u > 1.0f) return false;

		glm::vec3 q = glm::cross(s, edge1);
		float v = f * glm::dot(dir, q);

		if (v < 0.0f || u + v > 1.0f) return false;

		t = f * glm::dot(edge2, q);
		return t > EPSILON;
	}

} // namespace Boidsish
