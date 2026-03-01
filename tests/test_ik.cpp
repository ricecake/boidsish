#include <iostream>
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

    // Test twist minimization
    // Rotate root 45 degrees around Y. Chain should follow without twisting mid/end.
    model.ResetBones();
    glm::mat4 rotRoot = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    model.GetAnimator()->SetBoneLocalTransform("root", rotRoot);
    model.SolveIK("end", glm::vec3(1, 1, 1));

    glm::quat midRot = glm::quat_cast(model.GetAnimator()->GetBoneLocalTransform("mid"));
    float twist = glm::degrees(glm::angle(midRot));
    std::cout << "Mid Bone Twist Angle: " << twist << " degrees" << std::endl;

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

int main() {
    test_ik();
    return 0;
}
