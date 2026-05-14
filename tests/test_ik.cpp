#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <ik/body.h>
#include <ik/solver.hpp>

using namespace Boidsish;

void test_ik_basic() {
    Body body;
    body.position = glm::vec3(0, 0, 0);

    Chain chain;
    chain.base = glm::vec3(0, 0, 0);

    // Bone 1: length 1, starts at 0,0,0
    Bone bone1;
    bone1.name = "bone1";
    bone1.length = 1.0f;
    bone1.position = glm::vec3(0, 1, 0);
    chain.bones.push_back(bone1);

    // Bone 2: length 1, starts at 0,1,0
    Bone bone2;
    bone2.name = "bone2";
    bone2.length = 1.0f;
    bone2.position = glm::vec3(0, 2, 0);
    chain.bones.push_back(bone2);

    chain.target = glm::vec3(0, 1, 1);
    chain.hasTarget = true;

    body.tree.chains.push_back(chain);

    IKSolver::Solve(body, 20, 0.001f);

    glm::vec3 finalPos = body.tree.chains[0].bones.back().position;
    std::cout << "Final Effector Pos: " << finalPos.x << ", " << finalPos.y << ", " << finalPos.z << std::endl;

    float dist = glm::distance(finalPos, chain.target);
    if (dist < 0.01f) {
        std::cout << "Basic IK Success! Dist: " << dist << std::endl;
    } else {
        std::cout << "Basic IK Failed! Dist: " << dist << std::endl;
        exit(1);
    }
}

void test_ik_multi_effector() {
    Body body;
    body.position = glm::vec3(0, 0, 0);

    // Chain 1: Pulling +X
    Chain chain1;
    chain1.base = glm::vec3(0, 0, 0);
    Bone b1; b1.length = 1.0f; b1.position = glm::vec3(1, 0, 0);
    chain1.bones.push_back(b1);
    chain1.target = glm::vec3(2, 0, 0);
    chain1.hasTarget = true;

    // Chain 2: Pulling -X
    Chain chain2;
    chain2.base = glm::vec3(0, 0, 0);
    Bone b2; b2.length = 1.0f; b2.position = glm::vec3(-1, 0, 0);
    chain2.bones.push_back(b2);
    chain2.target = glm::vec3(-2, 0, 0);
    chain2.hasTarget = true;

    body.tree.chains.push_back(chain1);
    body.tree.chains.push_back(chain2);

    IKSolver::Solve(body, 20, 0.001f);

    std::cout << "Body position after balanced pull: " << body.position.x << ", " << body.position.y << ", " << body.position.z << std::endl;

    if (glm::length(body.position) < 0.01f) {
        std::cout << "Multi-effector Balance Success!" << std::endl;
    } else {
        std::cout << "Multi-effector Balance Failed!" << std::endl;
        exit(1);
    }
}

void test_ik_relative() {
    Body body;
    body.position = glm::vec3(1, 1, 1);

    Chain chain;
    chain.base = glm::vec3(0, 0, 0); // Relative to body.position

    Bone bone;
    bone.isRelative = true;
    bone.relativePosition = glm::vec3(0, 1, 0); // Relative to chain base
    bone.length = 1.0f;
    chain.bones.push_back(bone);

    body.tree.chains.push_back(chain);

    IKSolver::ResolveWorldPositions(body);

    glm::vec3 expectedPos = glm::vec3(1, 2, 1);
    float dist = glm::distance(body.tree.chains[0].bones[0].position, expectedPos);

    std::cout << "Relative Bone World Pos: " << body.tree.chains[0].bones[0].position.x << ", " << body.tree.chains[0].bones[0].position.y << ", " << body.tree.chains[0].bones[0].position.z << std::endl;

    if (dist < 0.001f) {
        std::cout << "Relative Position Resolve Success!" << std::endl;
    } else {
        std::cout << "Relative Position Resolve Failed!" << std::endl;
        exit(1);
    }
}

int main() {
    test_ik_basic();
    test_ik_multi_effector();
    test_ik_relative();
    std::cout << "All IK tests passed." << std::endl;
    return 0;
}
