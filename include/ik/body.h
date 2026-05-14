#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {
	class Constraint {
		int       type;
		glm::quat orientation;
		float     angle;
	};

	class Joint {
		float                   force;
		glm::quat               orientation;
		std::vector<Constraint> constraints;
	};

	class Bone {
		float              weight;
		float              length;
		glm::vec3          position;
		glm::quat          orientation;
		std::vector<Joint> joints;
	};

	class Chain {
		glm::vec3         base;
		std::vector<Bone> bones;
	};

	class Tree {
		std::vector<Chain> chains;
	};

	class Body {
		float     weight;
		Tree      tree;
		glm::vec3 center_of_mass;
		glm::vec3 goal;
	};

	Vector3 ApplyHingeConstraint(const Vector3& currentPos,
	                             const Vector3& previousPos,
	                             const Vector3& hingeNormal,
	                             const Vector3& referenceAxis,
	                             float          minAngleRad,
	                             float          maxAngleRad,
	                             float          boneLength) {
		Vector3 boneDir = (currentPos - previousPos).normalized();

		// 1. Project direction onto the hinge plane
		Vector3 projectedDir = boneDir - (boneDir.dot(hingeNormal) * hingeNormal);
		if (projectedDir.lengthSquared() < 0.0001f) {
			projectedDir = referenceAxis; // Fallback to avoid singularity
		} else {
			projectedDir.normalize();
		}

		// 2. Enforce Angular Limits (Knee limits)
		// Compute signed angle between referenceAxis and projectedDir
		float angle = atan2(projectedDir.cross(referenceAxis).dot(hingeNormal), projectedDir.dot(referenceAxis));

		// Clamp the angle
		float clampedAngle = std::clamp(angle, minAngleRad, maxAngleRad);

		// 3. Reconstruct constrained direction using quaternions/axis-angle
		Quaternion limitRot = Quaternion::AngleAxis(clampedAngle, hingeNormal);
		Vector3    constrainedDir = limitRot * referenceAxis;

		return previousPos + (constrainedDir * boneLength);
	}

	Vector3 ApplyConeConstraint(const Vector3& currentPos,
	                            const Vector3& previousPos,
	                            const Vector3& coneAxis,
	                            float          maxAngleRad,
	                            float          boneLength) {
		Vector3 boneDir = (currentPos - previousPos).normalized();

		float dotP = std::clamp(boneDir.dot(coneAxis), -1.0f, 1.0f);
		float currentAngle = acos(dotP);

		if (currentAngle > maxAngleRad) {
			// Calculate the axis of rotation perpendicular to both vectors
			Vector3 rotationAxis = coneAxis.cross(boneDir);

			if (rotationAxis.lengthSquared() < 0.0001f) {
				// Handle anti-parallel edge case
				rotationAxis = Vector3(1, 0, 0); // Or an orthogonal vector to coneAxis
			} else {
				rotationAxis.normalize();
			}

			// Clamp the direction to the edge of the cone
			Quaternion limitRot = Quaternion::AngleAxis(maxAngleRad, rotationAxis);
			Vector3    constrainedDir = limitRot * coneAxis;

			return previousPos + (constrainedDir * boneLength);
		}

		return currentPos; // Already within the cone
	}

	void ApplyTwistConstraint(Transform& parentTransform, Transform& childTransform, float maxTwistRad) {
		Vector3 boneDir = childTransform.forward;

		// Project the parent's up vector onto the child's local plane
		Vector3 parentUpProjected = parentTransform.up - (parentTransform.up.dot(boneDir) * boneDir);
		parentUpProjected.normalize();

		// Calculate twist angle
		float twistAngle = atan2(childTransform.up.cross(parentUpProjected).dot(boneDir),
		                         childTransform.up.dot(parentUpProjected));

		if (std::abs(twistAngle) > maxTwistRad) {
			float allowedTwist = (twistAngle > 0) ? maxTwistRad : -maxTwistRad;

			// Rotate the child's up vector to the limit boundary
			Quaternion twistCorrection = Quaternion::AngleAxis(allowedTwist - twistAngle, boneDir);
			childTransform.up = twistCorrection * childTransform.up;
			childTransform.right = boneDir.cross(childTransform.up);
		}
	}

} // namespace Boidsish