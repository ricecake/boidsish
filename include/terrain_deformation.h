#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Type of terrain deformation
	 */
	enum class DeformationType {
		Additive,   // Raises terrain (e.g., mounds, ridges)
		Subtractive // Lowers terrain (e.g., craters, trenches)
	};

	/**
	 * @brief Serializable description of a deformation for recreation
	 *
	 * Contains all parameters needed to recreate a deformation at a different location
	 * or serialize/deserialize the deformation state.
	 */
	struct DeformationDescriptor {
		std::string     type_name;        // Class type identifier (e.g., "Crater", "FlattenSquare")
		glm::vec3       center{0.0f};     // World-space center position
		glm::vec3       dimensions{0.0f}; // Size parameters (interpretation varies by type)
		glm::vec4       parameters{0.0f}; // Additional type-specific parameters
		uint32_t        seed = 0;         // Random seed for reproducible irregularity
		float           intensity = 1.0f; // Strength multiplier
		DeformationType deformation_type = DeformationType::Subtractive;
	};

	/**
	 * @brief Result of applying a deformation at a specific point
	 */
	struct DeformationResult {
		float     height_delta = 0.0f; // Change in height (positive = up, negative = down)
		glm::vec3 normal_offset{0.0f}; // Offset to apply to the normal (before renormalization)
		float     blend_weight = 0.0f; // 0-1, how strongly this deformation affects the point
		bool      applies = false;     // Whether this deformation affects the queried point
	};

	/**
	 * @brief Abstract base class for terrain deformations
	 *
	 * Deformations modify terrain height and normals within a bounded region.
	 * Each deformation stores a descriptor that can recreate it, enabling
	 * serialization and spatial queries.
	 *
	 * Thread Safety: Individual deformations are immutable after creation.
	 * The DeformationManager handles synchronization for the collection.
	 */
	class TerrainDeformation {
	public:
		explicit TerrainDeformation(uint32_t id): id_(id) {}

		virtual ~TerrainDeformation() = default;

		// Non-copyable, non-movable (referenced by ID in the voxel tree)
		TerrainDeformation(const TerrainDeformation&) = delete;
		TerrainDeformation& operator=(const TerrainDeformation&) = delete;
		TerrainDeformation(TerrainDeformation&&) = delete;
		TerrainDeformation& operator=(TerrainDeformation&&) = delete;

		/**
		 * @brief Get the unique identifier for this deformation
		 */
		uint32_t GetId() const { return id_; }

		/**
		 * @brief Get the deformation type (additive or subtractive)
		 */
		virtual DeformationType GetType() const = 0;

		/**
		 * @brief Get the type name for serialization
		 */
		virtual std::string GetTypeName() const = 0;

		/**
		 * @brief Get the axis-aligned bounding box containing this deformation
		 * @param out_min Minimum corner of AABB
		 * @param out_max Maximum corner of AABB
		 */
		virtual void GetBounds(glm::vec3& out_min, glm::vec3& out_max) const = 0;

		/**
		 * @brief Get the center position of this deformation
		 */
		virtual glm::vec3 GetCenter() const = 0;

		/**
		 * @brief Check if a point is within the deformation's area of effect
		 * @param world_pos World-space position (x, y, z) - y may be ignored for XZ-only checks
		 */
		virtual bool ContainsPoint(const glm::vec3& world_pos) const = 0;

		/**
		 * @brief Check if a 2D point (XZ plane) is within the deformation's footprint
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 */
		virtual bool ContainsPointXZ(float x, float z) const = 0;

		/**
		 * @brief Compute the height delta at a world position
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @param current_height The terrain's current height at this point (before deformation)
		 * @return Height change to apply (positive = raise, negative = lower)
		 */
		virtual float ComputeHeightDelta(float x, float z, float current_height) const = 0;

		/**
		 * @brief Transform a surface normal based on the deformation
		 *
		 * This method should be called after height modification to correct
		 * the surface normal for proper lighting.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @param original_normal The terrain's original surface normal
		 * @return The transformed normal (normalized)
		 */
		virtual glm::vec3 TransformNormal(float x, float z, const glm::vec3& original_normal) const = 0;

		/**
		 * @brief Get complete deformation result at a point
		 *
		 * Combines height delta, normal transformation, and blend weight.
		 *
		 * @param x World X coordinate
		 * @param z World Z coordinate
		 * @param current_height Current terrain height
		 * @param current_normal Current terrain normal
		 * @return DeformationResult with all computed values
		 */
		virtual DeformationResult
		ComputeDeformation(float x, float z, float current_height, const glm::vec3& current_normal) const = 0;

		/**
		 * @brief Get the descriptor that can recreate this deformation
		 */
		virtual DeformationDescriptor GetDescriptor() const = 0;

		/**
		 * @brief Get the maximum radius of effect from center (for spatial queries)
		 */
		virtual float GetMaxRadius() const = 0;

	protected:
		uint32_t id_;
	};

} // namespace Boidsish
