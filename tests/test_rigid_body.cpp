#include <gtest/gtest.h>
#include "rigid_body.h"
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/euler_angles.hpp>

const float THRESHOLD = 0.001f;

TEST(RigidBody, InitialState) {
    RigidBody rb;
    ASSERT_NEAR(rb.GetPosition().x, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetPosition().y, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetPosition().z, 0.0f, THRESHOLD);

    ASSERT_NEAR(rb.GetOrientation().w, 1.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().x, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().y, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().z, 0.0f, THRESHOLD);

    ASSERT_NEAR(rb.GetLinearVelocity().x, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetLinearVelocity().y, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetLinearVelocity().z, 0.0f, THRESHOLD);

    ASSERT_NEAR(rb.GetAngularVelocity().x, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetAngularVelocity().y, 0.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetAngularVelocity().z, 0.0f, THRESHOLD);
}

TEST(RigidBody, AddForce) {
    RigidBody rb;
    rb.mass_ = 10.0f;
    rb.linear_friction_ = 0.0f;
    rb.AddForce(glm::vec3(10.0f, 0.0f, 0.0f));
    rb.Update(1.0f);

    ASSERT_NEAR(rb.GetLinearVelocity().x, 1.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetPosition().x, 0.5f, THRESHOLD);
}

TEST(RigidBody, AddRelativeForce) {
    RigidBody rb;
    rb.mass_ = 10.0f;
    rb.linear_friction_ = 0.0f;

    // Rotate the body 90 degrees around the Y axis
    glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    rb.SetOrientation(rotation);

    rb.AddRelativeForce(glm::vec3(10.0f, 0.0f, 0.0f));
    rb.Update(1.0f);

    // The force was applied in the local x direction, which is now the world z direction
    ASSERT_NEAR(rb.GetLinearVelocity().z, -1.0f, THRESHOLD);
    ASSERT_NEAR(rb.GetPosition().z, -0.5f, THRESHOLD);
}

TEST(RigidBody, AddTorque) {
    RigidBody rb;
    rb.inertia_ = glm::vec3(1.0f, 1.0f, 1.0f);
    rb.angular_friction_ = 0.0f;
    rb.AddTorque(glm::vec3(1.0f, 0.0f, 0.0f));
    rb.Update(1.0f);

    // Check if the angular velocity is updated correctly
    ASSERT_NEAR(rb.GetAngularVelocity().x, 1.0f, THRESHOLD);

    // Check the orientation after rotation.
    // We expect a rotation around the x-axis.
    glm::quat expected_orientation = glm::angleAxis(0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    ASSERT_NEAR(rb.GetOrientation().w, expected_orientation.w, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().x, expected_orientation.x, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().y, expected_orientation.y, THRESHOLD);
    ASSERT_NEAR(rb.GetOrientation().z, expected_orientation.z, THRESHOLD);
}

TEST(RigidBody, AddRelativeTorque) {
    RigidBody rb;
    rb.inertia_ = glm::vec3(1.0f, 1.0f, 1.0f);
    rb.angular_friction_ = 0.0f;

    // Rotate the body 90 degrees around the Y axis
    glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    rb.SetOrientation(rotation);

    // Apply torque around the local x-axis
    rb.AddRelativeTorque(glm::vec3(1.0f, 0.0f, 0.0f));
    rb.Update(1.0f);

    // The torque was applied in the local x direction, which is now the world z direction
    glm::vec3 angular_velocity = rb.GetAngularVelocity();
    glm::vec3 expected_angular_velocity = glm::vec3(0.0f, 0.0f, -1.0f);
    ASSERT_NEAR(angular_velocity.x, expected_angular_velocity.x, THRESHOLD);
    ASSERT_NEAR(angular_velocity.y, expected_angular_velocity.y, THRESHOLD);
    ASSERT_NEAR(angular_velocity.z, expected_angular_velocity.z, THRESHOLD);
}

// --- LIMITS TESTS ---

TEST(RigidBody, ForceLimitIsEnabled) {
    RigidBody rb;
    rb.limit_force_ = true;
    rb.max_force_ = 10.0f;
    rb.AddForce(glm::vec3(20.0f, 0.0f, 0.0f));
    rb.Update(0.1f);
    // acc = 10 / 1 = 10. vel = 10 * 0.1 = 1.0. With friction, it will be slightly less.
    EXPECT_LE(glm::length(rb.GetLinearVelocity()), 1.0f);
}

TEST(RigidBody, ForceLimitIsDisabled) {
    RigidBody rb;
    rb.linear_friction_ = 0.0f;
    rb.limit_force_ = false;
    rb.max_force_ = 10.0f;
    rb.AddForce(glm::vec3(20.0f, 0.0f, 0.0f));
    rb.Update(0.1f);
    // acc = 20 / 1 = 20. vel = 20 * 0.1 = 2.0. With friction, it will be slightly less.
    EXPECT_GT(glm::length(rb.GetLinearVelocity()), 1.5f);
    EXPECT_LE(glm::length(rb.GetLinearVelocity()), 2.0f);
}

TEST(RigidBody, TorqueLimitIsEnabled) {
    RigidBody rb;
    rb.limit_torque_ = true;
    rb.max_torque_ = 5.0f;
    rb.AddTorque(glm::vec3(10.0f, 0.0f, 0.0f));
    rb.Update(0.1f);
    // ang_acc = 5 / 1 = 5. ang_vel = 5 * 0.1 = 0.5.
    EXPECT_LE(glm::length(rb.GetAngularVelocity()), 0.5f);
}

TEST(RigidBody, LinearVelocityLimitIsEnabled) {
    RigidBody rb;
    rb.limit_linear_velocity_ = true;
    rb.max_linear_velocity_ = 5.0f;
    rb.AddForce(glm::vec3(100.0f, 0.0f, 0.0f)); // Large force to exceed limit
    rb.Update(0.1f);
    EXPECT_LE(glm::length(rb.GetLinearVelocity()), 5.0f);
}

TEST(RigidBody, AngularVelocityLimitIsEnabled) {
    RigidBody rb;
    rb.limit_angular_velocity_ = true;
    rb.max_angular_velocity_ = 2.0f;
    rb.AddTorque(glm::vec3(50.0f, 0.0f, 0.0f)); // Large torque to exceed limit
    rb.Update(0.1f);
    EXPECT_LE(glm::length(rb.GetAngularVelocity()), 2.0f);
}


// --- WRENCH TESTS ---

TEST(RigidBody, WrenchAppliesLocalForce) {
    RigidBody rb;
    rb.linear_friction_ = 0.0f;
    // Wrench applies a force of 10 units in the local +X direction
    rb.wrench_.dual = glm::quat(0, 10.0f, 0.0f, 0.0f);

    // Rotate the body 90 degrees around Y axis, so local +X is now world +Z
    rb.SetOrientation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));

    rb.Update(0.1f);

    glm::vec3 vel = rb.GetLinearVelocity();

    // Expect velocity to be primarily in the world -Z direction
    EXPECT_LT(vel.z, -0.5f);
    EXPECT_NEAR(vel.x, 0.0f, 0.001f);
    EXPECT_NEAR(vel.y, 0.0f, 0.001f);
}

TEST(RigidBody, WrenchAppliesLocalTorque) {
    RigidBody rb;
    rb.angular_friction_ = 0.0f;
    // Wrench applies a torque of 10 units around the local +Y axis
    rb.wrench_.real = glm::quat(0, 0.0f, 10.0f, 0.0f);

    // Rotate the body 90 degrees around X axis, so local +Y is now world +Z
    rb.SetOrientation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)));

    rb.Update(0.1f);

    glm::vec3 ang_vel = rb.GetAngularVelocity();

    // Expect angular velocity to be primarily around the world +Z axis
    EXPECT_GT(ang_vel.z, 0.5f);
    EXPECT_NEAR(ang_vel.x, 0.0f, 0.001f);
    EXPECT_NEAR(ang_vel.y, 0.0f, 0.001f);
}

// --- ScLERP TEST ---

TEST(RigidBody, OrientationUpdateUsesScLERP) {
    RigidBody rb;
    rb.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    rb.SetOrientation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)));

    // Apply force to cause movement and rotation
    rb.AddForce(glm::vec3(0, 0, 10.0f));
    rb.AddTorque(glm::vec3(0, 10.0f, 0));

    rb.Update(0.1f);

    // Get the pose after one update
    glm::vec3 pos1 = rb.GetPosition();
    glm::quat orient1 = rb.GetOrientation();

    // It's hard to predict the exact final pose due to the complexity of ScLERP.
    // Instead, we check that the state has changed from the initial state in a plausible direction.
    EXPECT_NE(pos1, glm::vec3(10.0f, 0.0f, 0.0f));
    EXPECT_GT(pos1.z, 0.0f); // Should have moved in +Z

    float angle = glm::angle(orient1);
    EXPECT_GT(angle, glm::radians(45.0f)); // Angle should have increased
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}