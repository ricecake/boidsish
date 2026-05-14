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
	inline void ApplyTwistConstraint(glm::quat& boneOrientation, const glm::vec3& boneDir, const glm::vec3& parentUp, float maxTwistRad) {
		glm::vec3 currentUp = boneOrientation * glm::vec3(0, 1, 0);

		// Project vectors onto plane perpendicular to boneDir
		glm::vec3 parentUpProjected = parentUp - glm::dot(parentUp, boneDir) * boneDir;
		if (glm::length(parentUpProjected) < 0.0001f) return;
		parentUpProjected = glm::normalize(parentUpProjected);

		glm::vec3 currentUpProjected = currentUp - glm::dot(currentUp, boneDir) * boneDir;
		if (glm::length(currentUpProjected) < 0.0001f) return;
		currentUpProjected = glm::normalize(currentUpProjected);

		float twistAngle = atan2(glm::dot(glm::cross(parentUpProjected, currentUpProjected), boneDir),
		                         glm::dot(parentUpProjected, currentUpProjected));

		if (std::abs(twistAngle) > maxTwistRad) {
			float allowedTwist = (twistAngle > 0) ? maxTwistRad : -maxTwistRad;
			glm::quat correction = glm::angleAxis(allowedTwist - twistAngle, boneDir);
			boneOrientation = correction * boneOrientation;
		}
	}

} // namespace Boidsish

#endif
