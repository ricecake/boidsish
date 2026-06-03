#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include "model.h"
#include "animator.h"

using namespace Boidsish;

// Helper to create a basic 3-bone chain along Y axis
std::shared_ptr<ModelData> CreateSimpleChain() {
    auto data = std::make_shared<ModelData>();
    data->model_path = "simple_chain";

    // root (0,0,0) -> mid (0,1,0) -> end (0,2,0)
    data->AddBone("root", "", glm::mat4(1.0f));
    data->AddBone("mid", "root", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0)));
    data->AddBone("end", "mid", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0)));

    return data;
}

// Helper to calculate angle between two bones in degrees
float GetJointAngle(const Model& model, const std::string& parentName, const std::string& childName, const glm::vec3& referenceDir) {
    glm::vec3 parentPos = model.GetBoneWorldPosition(parentName);
    glm::vec3 childPos = model.GetBoneWorldPosition(childName);
    glm::vec3 dir = glm::normalize(childPos - parentPos);

    float dot = glm::clamp(glm::dot(dir, referenceDir), -1.0f, 1.0f);
    return glm::degrees(std::acos(dot));
}

TEST(IKRobustTest, BasicReachability) {
    auto data = CreateSimpleChain();
    Model model(data);
    model.SetPosition(0, 0, 0);
    model.UpdateAnimation(0.0f);

    // Target within reach
    glm::vec3 target(0, 1, 1);
    model.SolveIK("end", target);

    glm::vec3 endPos = model.GetBoneWorldPosition("end");
    EXPECT_NEAR(glm::distance(endPos, target), 0.0f, 0.01f);

    // Target out of reach
    glm::vec3 farTarget(0, 5, 0);
    model.SolveIK("end", farTarget);
    endPos = model.GetBoneWorldPosition("end");
    EXPECT_NEAR(glm::distance(endPos, glm::vec3(0, 2, 0)), 0.0f, 0.01f);
}

TEST(IKRobustTest, HingeConstraintX) {
    auto data = CreateSimpleChain();
    Model model(data);
    model.UpdateAnimation(0.0f);

    BoneConstraint hinge;
    hinge.type = ConstraintType::Hinge;
    hinge.axis = glm::vec3(1, 0, 0);
    hinge.minAngle = -30.0f;
    hinge.maxAngle = 30.0f;
    hinge.restAngle = 0.0f;
    model.SetBoneConstraint("mid", hinge);

    glm::vec3 target(0, 0, 2);
    model.SolveIK("end", target, 0.01f, 50);

    glm::vec3 rootPos = model.GetBoneWorldPosition("root");
    glm::vec3 midPos = model.GetBoneWorldPosition("mid");
    glm::vec3 dir = glm::normalize(midPos - rootPos);

    float angleMid = GetJointAngle(model, "mid", "end", dir);
    EXPECT_LE(angleMid, 30.1f);
}

TEST(IKRobustTest, ConeConstraint) {
    auto data = CreateSimpleChain();
    Model model(data);
    model.UpdateAnimation(0.0f);

    BoneConstraint cone;
    cone.type = ConstraintType::Cone;
    cone.coneAngle = 20.0f;
    model.SetBoneConstraint("mid", cone);

    glm::vec3 target(2, 1, 0);
    model.SolveIK("end", target, 0.01f, 50);

    glm::vec3 rootPos = model.GetBoneWorldPosition("root");
    glm::vec3 midPos = model.GetBoneWorldPosition("mid");
    glm::vec3 midDir = glm::normalize(midPos - rootPos);

    float angle = GetJointAngle(model, "mid", "end", midDir);
    EXPECT_LE(angle, 20.1f);
}

TEST(IKRobustTest, TwistConstraint) {
    auto data = CreateSimpleChain();
    Model model(data);
    model.UpdateAnimation(0.0f);

    BoneConstraint twist;
    twist.minTwist = -10.0f;
    twist.maxTwist = 10.0f;
    model.SetBoneConstraint("mid", twist);

    glm::vec3 target(0.5f, 1.5f, 0.5f);
    model.SolveIK("end", target);

    glm::quat midRot = model.GetBoneWorldRotation("mid");
    glm::vec3 bindDir(0, 1, 0);

    glm::vec3 qv(midRot.x, midRot.y, midRot.z);
    glm::vec3 proj = glm::dot(qv, bindDir) * bindDir;
    glm::quat twistQ(midRot.w, proj.x, proj.y, proj.z);
    if (glm::length(twistQ) > 0) {
        twistQ = glm::normalize(twistQ);
        if (twistQ.w < 0) twistQ = -twistQ;
        float twistAngle = glm::degrees(2.0f * std::atan2(glm::length(glm::vec3(twistQ.x, twistQ.y, twistQ.z)), twistQ.w));
        EXPECT_LE(std::abs(twistAngle), 10.1f);
    }
}

TEST(IKRobustTest, MultiEffector) {
    auto data = std::make_shared<ModelData>();
    data->model_path = "y_shape";
    data->AddBone("root", "", glm::mat4(1.0f));
    data->AddBone("spine", "root", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0)));

    data->AddBone("armL_1", "spine", glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0)));
    data->AddBone("armL_2", "armL_1", glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0)));

    data->AddBone("armR_1", "spine", glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.5f, 0)));
    data->AddBone("armR_2", "armR_1", glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.5f, 0)));

    Model model(data);
    model.UpdateAnimation(0.0f);

    std::vector<std::string> effectors = {"armL_2", "armR_2"};
    std::vector<glm::vec3> targets = {glm::vec3(0.5f, 2.0f, 0.5f), glm::vec3(-0.5f, 2.0f, -0.5f)};

    model.SolveIK(effectors, targets, 0.01f, 50);

    EXPECT_NEAR(glm::distance(model.GetBoneWorldPosition("armL_2"), targets[0]), 0.0f, 0.1f);
    EXPECT_NEAR(glm::distance(model.GetBoneWorldPosition("armR_2"), targets[1]), 0.0f, 0.1f);
}

TEST(IKRobustTest, FreeFloatingBody) {
    auto data = CreateSimpleChain();
    Model model(data);
    model.UpdateAnimation(0.0f);

    glm::vec3 target(0, 5, 0);
    model.SolveIK("end", target, 0.01f, 50, "root", {}, true);

    glm::vec3 endPos = model.GetBoneWorldPosition("end");
    EXPECT_NEAR(glm::distance(endPos, target), 0.0f, 0.1f);
    EXPECT_GT(glm::distance(model.GetBoneWorldPosition("root"), glm::vec3(0,0,0)), 0.1f);
}

TEST(IKRobustTest, SpiderGait) {
    auto data = std::make_shared<ModelData>();
    data->model_path = "spider_model";
    data->AddBone("hub", "", glm::mat4(1.0f));

    auto addLeg = [&](const std::string& name, const glm::vec3& hipOffset) {
        data->AddBone(name + "_hip", "hub", glm::translate(glm::mat4(1.0f), hipOffset));
        data->AddBone(name + "_knee", name + "_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.5f, 0.5f)));
        data->AddBone(name + "_foot", name + "_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.5f, 0.5f)));
    };

    addLeg("FL", glm::vec3( 1, 0,  1));
    addLeg("FR", glm::vec3(-1, 0,  1));
    addLeg("BL", glm::vec3( 1, 0, -1));
    addLeg("BR", glm::vec3(-1, 0, -1));

    Model model(data);
    model.SetPosition(0, 0.5f, 0);
    model.UpdateAnimation(0.0f);

    std::vector<std::string> legFeet = {"FL_foot", "FR_foot", "BL_foot", "BR_foot"};
    std::vector<glm::vec3> footTargets;
    for (const auto& foot : legFeet) footTargets.push_back(model.GetBoneWorldPosition(foot));

    glm::vec3 startHubPos = model.GetBoneWorldPosition("hub");

    // Move foot targets forward in Z
    for (int step = 0; step < 50; ++step) {
        for (auto& target : footTargets) {
            target.z += 0.1f;
        }
        model.SolveIK(legFeet, footTargets, 0.01f, 20, "hub", {}, true);
        model.UpdateAnimation(0.0f);
    }

    glm::vec3 endHubPos = model.GetBoneWorldPosition("hub");

    // Hub should have moved forward significantly (approx 5.0 units)
    EXPECT_GT(endHubPos.z - startHubPos.z, 3.5f);

    for (const auto& foot : legFeet) {
        glm::vec3 fPos = model.GetBoneWorldPosition(foot);
        EXPECT_FALSE(std::isnan(fPos.x));
    }
}
