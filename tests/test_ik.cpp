#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "model.h"
#include "animator.h"
#include "logger.h"

using namespace Boidsish;

void test_ik() {
    auto data = std::make_shared<ModelData>();
    data->model_path = "test_ik_model";

    // Create a simple 3-bone chain: root -> mid -> end
    // Each bone is 1.0 units long along Y axis
    glm::mat4 rootLocal = glm::mat4(1.0f);
    data->AddBone("root", "", rootLocal);

    glm::mat4 midLocal = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0));
    data->AddBone("mid", "root", midLocal);

    glm::mat4 endLocal = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0));
    data->AddBone("end", "mid", endLocal);

    Model model(data);
    model.SetPosition(0, 0, 0);
    model.UpdateAnimation(0); // Initial pose

    std::cout << "Initial Effector Pos: " << model.GetBoneWorldPosition("end").x << ", "
              << model.GetBoneWorldPosition("end").y << ", "
              << model.GetBoneWorldPosition("end").z << std::endl;

    // Target position: 2 units in front (Z) and 1 unit up (Y)
    glm::vec3 target(0, 1, 1);
    model.SolveIK("end", target);

    glm::vec3 finalPos = model.GetBoneWorldPosition("end");
    std::cout << "Final Effector Pos: " << finalPos.x << ", " << finalPos.y << ", " << finalPos.z << std::endl;

    float dist = glm::distance(finalPos, target);
    if (dist < 0.1f) {
        std::cout << "IK Success! Dist: " << dist << std::endl;
    } else {
        std::cout << "IK Failed! Dist: " << dist << std::endl;
        exit(1);
    }

    // Test constraints
    BoneConstraint hinge;
    hinge.type = ConstraintType::Hinge;
    hinge.axis = glm::vec3(1, 0, 0); // Rotate around X
    hinge.minAngle = -45.0f;
    hinge.maxAngle = 45.0f;
    model.SetBoneConstraint("mid", hinge);

    target = glm::vec3(0, 0, 2); // Out of reach but in reach if straight
    model.SolveIK("end", target);
    finalPos = model.GetBoneWorldPosition("end");
    std::cout << "Constrained Effector Pos: " << finalPos.x << ", " << finalPos.y << ", " << finalPos.z << std::endl;
}

void test_twist_constraints() {
    auto data = std::make_shared<ModelData>();
    data->model_path = "test_twist_model";

    // 3-bone chain along Y: root(0,0,0) -> mid(0,1,0) -> end(0,2,0)
    data->AddBone("root", "", glm::mat4(1.0f));
    data->AddBone("mid", "root", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0)));
    data->AddBone("end", "mid", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0)));

    Model model(data);
    model.SetPosition(0, 0, 0);
    model.UpdateAnimation(0);

    // Set tight twist limits on mid bone: ±5°
    BoneConstraint tc;
    tc.minTwist = -5.0f;
    tc.maxTwist = 5.0f;
    model.SetBoneConstraint("mid", tc);

    // Solve IK with a target that would induce twist if unconstrained.
    // Target off to the side — the minimum-arc rotation from (0,1,0) to a
    // diagonal direction naturally introduces some axial roll.
    glm::vec3 target(0.5f, 1.5f, 0.5f);
    model.SolveIK("end", target, 0.05f, 30);

    // Extract mid bone's world rotation and decompose twist around bind dir
    glm::quat midRot = model.GetBoneWorldRotation("mid");
    glm::vec3 bindDir(0, 1, 0); // bind direction of the mid->end segment

    // Swing-twist decomposition
    glm::vec3 qv(midRot.x, midRot.y, midRot.z);
    glm::vec3 proj = glm::dot(qv, bindDir) * bindDir;
    glm::quat twist(midRot.w, proj.x, proj.y, proj.z);
    float twistLen = glm::length(twist);
    float twistAngle = 0.0f;
    if (twistLen > 1e-6f) {
        twist /= twistLen;
        twistAngle = glm::degrees(
            2.0f * std::atan2(glm::dot(glm::vec3(twist.x, twist.y, twist.z), bindDir), twist.w)
        );
    }

    std::cout << "Twist angle on mid bone: " << twistAngle << " degrees" << std::endl;

    // Allow some numerical slack (1° beyond the limit)
    float slack = 1.0f;
    if (twistAngle < tc.minTwist - slack || twistAngle > tc.maxTwist + slack) {
        std::cout << "Twist constraint FAILED! Angle " << twistAngle
                  << " outside [" << tc.minTwist << ", " << tc.maxTwist << "]" << std::endl;
        exit(1);
    }
    std::cout << "Twist constraint OK" << std::endl;
}

int main() {
    test_ik();
    test_twist_constraints();
    std::cout << "All IK tests passed." << std::endl;
    return 0;
}
