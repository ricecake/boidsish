#ifndef BOIDSISH_IK_BODY_H
#define BOIDSISH_IK_BODY_H

#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "chain.hpp"

namespace Boidsish {

	class Tree {
	public:
		std::vector<Chain> chains;
	};

	class Body {
	public:
		float     weight = 1.0f;
		Tree      tree;
		glm::vec3 center_of_mass = glm::vec3(0.0f);
		glm::vec3 position = glm::vec3(0.0f);
		glm::vec3 goal = glm::vec3(0.0f);
	};

	inline glm::vec3 ApplyHingeConstraint(const glm::vec3& currentPos,
	                                     const glm::vec3& previousPos,
	                                     const glm::vec3& hingeNormal,
	                                     const glm::vec3& referenceAxis,
	                                     float          minAngleRad,
	                                     float          maxAngleRad,
	                                     float          boneLength) {
		glm::vec3 boneDir = glm::normalize(currentPos - previousPos);

		// 1. Project direction onto the hinge plane
		glm::vec3 projectedDir = boneDir - (glm::dot(boneDir, hingeNormal) * hingeNormal);
		if (glm::length(projectedDir) < 0.0001f) {
			projectedDir = referenceAxis; // Fallback to avoid singularity
		} else {
			projectedDir = glm::normalize(projectedDir);
		}

		// 2. Enforce Angular Limits (Knee limits)
		// Compute signed angle between referenceAxis and projectedDir
		float angle = atan2(glm::dot(glm::cross(referenceAxis, projectedDir), hingeNormal), glm::dot(projectedDir, referenceAxis));

		// Clamp the angle
		float clampedAngle = std::clamp(angle, minAngleRad, maxAngleRad);

		// 3. Reconstruct constrained direction using quaternions/axis-angle
		glm::quat limitRot = glm::angleAxis(clampedAngle, hingeNormal);
		glm::vec3 constrainedDir = limitRot * referenceAxis;

		return previousPos + (constrainedDir * boneLength);
	}

	inline glm::vec3 ApplyConeConstraint(const glm::vec3& currentPos,
	                                    const glm::vec3& previousPos,
	                                    const glm::vec3& coneAxis,
	                                    float          maxAngleRad,
	                                    float          boneLength) {
		glm::vec3 boneDir = glm::normalize(currentPos - previousPos);

		float dotP = std::clamp(glm::dot(boneDir, coneAxis), -1.0f, 1.0f);
		float currentAngle = acos(dotP);

		if (currentAngle > maxAngleRad) {
			// Calculate the axis of rotation perpendicular to both vectors
			glm::vec3 rotationAxis = glm::cross(coneAxis, boneDir);

			if (glm::length(rotationAxis) < 0.0001f) {
				// Handle anti-parallel edge case
				rotationAxis = glm::vec3(1, 0, 0); // Or an orthogonal vector to coneAxis
			} else {
				rotationAxis = glm::normalize(rotationAxis);
			}

			// Clamp the direction to the edge of the cone
			glm::quat limitRot = glm::angleAxis(maxAngleRad, rotationAxis);
			glm::vec3 constrainedDir = limitRot * coneAxis;

			return previousPos + (constrainedDir * boneLength);
		}

		return currentPos; // Already within the cone
	}

	// Simplified Twist Constraint helper
	// Replaces your truncated ApplyTwistConstraint in body.h
	inline void ApplyTwistConstraint(glm::quat& boneOrientation, const glm::vec3& boneDir, const glm::vec3& parentRefAxis, float maxTwistRad) {
		// 1. Get the child's current reference vector (Assuming Z is our orthogonal reference)
		glm::vec3 childRefAxis = boneOrientation * glm::vec3(0, 0, 1);

		// 2. Project both the parent and child reference axes onto the plane perpendicular to the bone direction
		glm::vec3 projParent = parentRefAxis - (glm::dot(parentRefAxis, boneDir) * boneDir);
		glm::vec3 projChild = childRefAxis - (glm::dot(childRefAxis, boneDir) * boneDir);

		// Guard against singularities if the reference axis aligns perfectly with the bone direction
		if (glm::length(projParent) < 0.0001f || glm::length(projChild) < 0.0001f) return;

		projParent = glm::normalize(projParent);
		projChild = glm::normalize(projChild);

		// 3. Calculate the signed twist angle using dot and cross products
		float angle = acos(std::clamp(glm::dot(projParent, projChild), -1.0f, 1.0f));

		// Determine direction of the twist
		glm::vec3 crossAxis = glm::cross(projParent, projChild);
		if (glm::dot(crossAxis, boneDir) < 0) {
			angle = -angle;
		}

		// 4. If the twist exceeds the limit, calculate the difference and counter-rotate the quaternion
		if (std::abs(angle) > maxTwistRad) {
			float clampedAngle = std::clamp(angle, -maxTwistRad, maxTwistRad);
			float correctionAngle = clampedAngle - angle; // The amount we need to push back

			// Create a correction rotation purely around the bone's directional axis
			glm::quat correctionRot = glm::angleAxis(correctionAngle, boneDir);

			// Apply the correction to the final orientation
			boneOrientation = correctionRot * boneOrientation;
		}
	}
} // namespace Boidsish

#endif
