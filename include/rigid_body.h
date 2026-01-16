#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

class RigidBody {
public:
    // Physical properties
    float mass_ = 1.0f;
    glm::vec3 inertia_ = glm::vec3(1.0f, 1.0f, 1.0f);
    float linear_friction_ = 0.1f;
    float angular_friction_ = 0.1f;

private:
	// Position + Orientation (Pose)
	glm::dualquat pose_;

	// Linear + Angular Velocity (Twist)
	glm::dualquat twist_;

    // Accumulated forces and torques for this frame
    glm::vec3 force_accumulator_ = glm::vec3(0.0f);
    glm::vec3 torque_accumulator_ = glm::vec3(0.0f);

public:
    // Constructors
	RigidBody(): pose_(glm::quat(1, 0, 0, 0), glm::vec3(0)), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position): pose_(glm::quat(1, 0, 0, 0), position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position, const glm::quat& orientation): pose_(orientation, position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};

    // Getters
    glm::vec3 GetPosition() const;
    glm::quat GetOrientation() const;
    glm::vec3 GetLinearVelocity() const;
    glm::vec3 GetAngularVelocity() const;
	std::string ToString() const;

    // Setters
    void SetPosition(const glm::vec3& position);
    void SetOrientation(const glm::quat& orientation);
    void SetLinearVelocity(const glm::vec3& velocity);
    void SetAngularVelocity(const glm::vec3& velocity);


	// Force and Torque Application
    void AddForce(const glm::vec3& force); // Add force in world coordinates
    void AddRelativeForce(const glm::vec3& force); // Add force in local coordinates

    void AddTorque(const glm::vec3& torque); // Add torque in world coordinates
    void AddRelativeTorque(const glm::vec3& torque); // Add torque in local coordinates

	// Main integration step
	void Update(float dt);

    // Utility
    void FaceVelocity();
};