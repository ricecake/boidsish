#ifndef BOIDSISH_IK_JOINT_HPP
#define BOIDSISH_IK_JOINT_HPP

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {
    enum class ConstraintType {
        None,
        Hinge,
        Cone,
        Twist
    };

    struct Constraint {
        ConstraintType type = ConstraintType::None;
        glm::quat      orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        float          angle = 0.0f; // For cone or hinge limit
        float          minAngle = 0.0f; // For hinge
        float          maxAngle = 0.0f; // For hinge
        float          restAngle = 0.0f; // For hinge
        glm::vec3      axis = glm::vec3(0.0f, 1.0f, 0.0f); // For hinge/twist axis
    };

    struct Joint {
        float                   force = 1.0f;
        glm::quat               orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        std::vector<Constraint> constraints;
    };
}

#endif
