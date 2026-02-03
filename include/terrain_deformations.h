#pragma once

#include <cmath>
#include <random>

#include "terrain_deformation.h"

namespace Boidsish {

	/**
	 * @brief Crater deformation - creates a bowl-shaped depression in terrain
	 *
	 * The crater has configurable radius, depth, and irregularity. The shape
	 * follows a smooth falloff from the rim to the center, with optional
	 * noise-based irregularity for a more natural appearance.
	 */
	class CraterDeformation: public TerrainDeformation {
	public:
		/**
		 * @brief Create a crater deformation
		 *
		 * @param id Unique identifier
		 * @param center Center position (Y component is the rim height)
		 * @param radius Radius of the crater
		 * @param depth Maximum depth at center (positive value = deeper crater)
		 * @param irregularity Amount of random variation (0-1, 0 = perfect circle)
		 * @param rim_height Height of raised rim around crater (0 = no rim)
		 * @param seed Random seed for reproducible irregularity
		 */
		CraterDeformation(
			uint32_t         id,
			const glm::vec3& center,
			float            radius,
			float            depth,
			float            irregularity = 0.0f,
			float            rim_height = 0.0f,
			uint32_t         seed = 0
		);

		DeformationType GetType() const override { return DeformationType::Subtractive; }

		std::string GetTypeName() const override { return "Crater"; }

		void GetBounds(glm::vec3& out_min, glm::vec3& out_max) const override;

		glm::vec3 GetCenter() const override { return center_; }

		float GetMaxRadius() const override { return radius_ + rim_width_; }

		bool ContainsPoint(const glm::vec3& world_pos) const override;
		bool ContainsPointXZ(float x, float z) const override;

		float     ComputeHeightDelta(float x, float z, float current_height) const override;
		glm::vec3 TransformNormal(float x, float z, const glm::vec3& original_normal) const override;

		DeformationResult
		ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const override;

		DeformationDescriptor GetDescriptor() const override;

		// Crater-specific getters
		float GetRadius() const { return radius_; }

		float GetDepth() const { return depth_; }

		float GetIrregularity() const { return irregularity_; }

		float GetRimHeight() const { return rim_height_; }

	private:
		/**
		 * @brief Compute the irregularity offset for an angle
		 */
		float GetIrregularityOffset(float angle) const;

		/**
		 * @brief Compute the crater profile (depth as function of normalized distance)
		 */
		float ComputeCraterProfile(float normalized_dist) const;

		glm::vec3 center_;
		float     radius_;
		float     depth_;
		float     irregularity_;
		float     rim_height_;
		float     rim_width_;
		uint32_t  seed_;

		// Precomputed irregularity values (for performance)
		static constexpr int kIrregularitySamples = 32;
		float                irregularity_samples_[kIrregularitySamples];
	};

	/**
	 * @brief Flatten square deformation - levels terrain to a specific height in a rectangular area
	 *
	 * Creates a flat platform at the specified Y level. Terrain within the footprint
	 * is adjusted (raised or lowered) to match the target height. Edges can be
	 * optionally blended for smoother transitions.
	 */
	class FlattenSquareDeformation: public TerrainDeformation {
	public:
		/**
		 * @brief Create a flatten square deformation
		 *
		 * @param id Unique identifier
		 * @param center Center position (Y component is the target height)
		 * @param half_width Half-width in X direction
		 * @param half_depth Half-depth in Z direction
		 * @param blend_distance Distance over which to blend to original terrain (0 = hard edge)
		 * @param rotation_y Rotation around Y axis in radians (0 = axis-aligned)
		 */
		FlattenSquareDeformation(
			uint32_t         id,
			const glm::vec3& center,
			float            half_width,
			float            half_depth,
			float            blend_distance = 0.0f,
			float            rotation_y = 0.0f
		);

		DeformationType GetType() const override {
			// Can be either additive or subtractive depending on terrain
			return DeformationType::Subtractive;
		}

		std::string GetTypeName() const override { return "FlattenSquare"; }

		void GetBounds(glm::vec3& out_min, glm::vec3& out_max) const override;

		glm::vec3 GetCenter() const override { return center_; }

		float GetMaxRadius() const override;

		bool ContainsPoint(const glm::vec3& world_pos) const override;
		bool ContainsPointXZ(float x, float z) const override;

		float     ComputeHeightDelta(float x, float z, float current_height) const override;
		glm::vec3 TransformNormal(float x, float z, const glm::vec3& original_normal) const override;

		DeformationResult
		ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const override;

		DeformationDescriptor GetDescriptor() const override;

		// Square-specific getters
		float GetHalfWidth() const { return half_width_; }

		float GetHalfDepth() const { return half_depth_; }

		float GetBlendDistance() const { return blend_distance_; }

		float GetRotationY() const { return rotation_y_; }

		float GetTargetHeight() const { return center_.y; }

	private:
		/**
		 * @brief Transform world coordinates to local (rotated) space
		 */
		glm::vec2 WorldToLocal(float x, float z) const;

		/**
		 * @brief Compute blend weight based on distance from edge
		 */
		float ComputeBlendWeight(float local_x, float local_z) const;

		glm::vec3 center_;
		float     half_width_;
		float     half_depth_;
		float     blend_distance_;
		float     rotation_y_;

		// Precomputed rotation values
		float cos_rot_;
		float sin_rot_;
	};

} // namespace Boidsish
