#ifndef BOIDSISH_IK_CHAIN_HPP
#define BOIDSISH_IK_CHAIN_HPP

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "joint.hpp"

namespace Boidsish {
    struct Bone {
        std::string        name;
        float              weight = 1.0f;
        float              length = 1.0f;

        // These are solved/current state in world space
        glm::vec3          position = glm::vec3(0.0f);
        glm::quat          orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        // Relative positioning for layout
        bool               isRelative = false;
        glm::vec3          relativePosition = glm::vec3(0.0f);

        // Bind-pose direction for constraint reference
        glm::vec3          bindDir = glm::vec3(0, 1, 0);

        std::vector<Joint> joints;
    };

    struct Chain {
        glm::vec3         base = glm::vec3(0.0f);
        std::vector<Bone> bones;

        // Effector target
        glm::vec3         target = glm::vec3(0.0f);
        bool              hasTarget = false;
    };
}

#endif
