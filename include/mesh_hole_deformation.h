#pragma once

#include <vector>
#include "terrain_deformation.h"
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Mesh hole deformation - cuts a hole in terrain where it intersects a mesh volume
	 *
	 * This deformation uses ray-casting against a provided set of triangles to determine
	 * if a point on the terrain is "inside" the mesh. If it is, the point is marked
	 * as a hole and discarded during rendering.
	 */
	class MeshHoleDeformation : public TerrainDeformation {
	public:
		/**
		 * @brief Create a mesh hole deformation
		 *
		 * @param id Unique identifier
		 * @param vertices Mesh vertices in world space
		 * @param indices Mesh indices
		 */
		MeshHoleDeformation(uint32_t id, const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indices);

		DeformationType GetType() const override { return DeformationType::Subtractive; }
		std::string GetTypeName() const override { return "MeshHole"; }

		void GetBounds(glm::vec3& out_min, glm::vec3& out_max) const override;
		glm::vec3 GetCenter() const override { return (min_bound_ + max_bound_) * 0.5f; }
		float GetMaxRadius() const override;

		bool ContainsPoint(const glm::vec3& world_pos) const override;
		bool ContainsPointXZ(float x, float z) const override;

		float ComputeHeightDelta(float x, float z, float current_height) const override;
		bool IsHole(float x, float z, float current_height) const override;

		glm::vec3 TransformNormal(float x, float z, const glm::vec3& original_normal) const override;

		DeformationResult ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const override;

		DeformationDescriptor GetDescriptor() const override;

	private:
		struct Triangle {
			glm::vec3 v0, v1, v2;
			glm::vec3 min_bound, max_bound;
		};

		std::vector<Triangle> triangles_;
		glm::vec3 min_bound_;
		glm::vec3 max_bound_;

		// Ray-triangle intersection test (Moeller-Trumbore)
		bool RayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir, const Triangle& tri, float& t) const;
	};

} // namespace Boidsish
