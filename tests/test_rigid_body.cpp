#include <gtest/gtest.h>
#include "rigid_body.h"
#include <glm/gtx/string_cast.hpp>

#define GLM_ENABLE_EXPERIMENTAL
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
