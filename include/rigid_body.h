#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

class RigidBody {
public:
    // Physical properties
    float mass_ = 1.0f;
    glm::vec3 inertia_ = glm::vec3(1.0f, 1.0f, 1.0f);
    float linear_friction_ = 7.5f;
    float angular_friction_ = 7.5f;

    // Optional limits
    bool limit_force_ = false;
    float max_force_ = 100.0f;
    bool limit_torque_ = false;
    float max_torque_ = 100.0f;
    bool limit_linear_velocity_ = false;
    float max_linear_velocity_ = 100.0f;
    bool limit_angular_velocity_ = false;
    float max_angular_velocity_ = 100.0f;

    // Persistent forces and torques (wrench) in local coordinates
    glm::dualquat wrench_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));

private:
	// Position + Orientation (Pose) in world coordinates
	glm::dualquat pose_;

	// Linear + Angular Velocity (Twist) in world coordinates
	glm::dualquat twist_;

    // Accumulated forces and torques for this frame in world coordinates
    glm::vec3 force_accumulator_ = glm::vec3(0.0f);
    glm::vec3 torque_accumulator_ = glm::vec3(0.0f);

public:
    // Constructors
	RigidBody(): pose_(glm::quat(1, 0, 0, 0), glm::vec3(0)), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position): pose_(glm::quat(1, 0, 0, 0), position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position, const glm::quat& orientation): pose_(orientation, position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};

    // Getters
    glm::vec3 GetPosition() const; // Returns position in world coordinates
    glm::quat GetOrientation() const; // Returns orientation in world coordinates
    glm::vec3 GetLinearVelocity() const; // Returns linear velocity in world coordinates
    glm::vec3 GetAngularVelocity() const; // Returns angular velocity in world coordinates
	std::string ToString() const;

    // Setters
    void SetPosition(const glm::vec3& position); // Sets position in world coordinates
    void SetOrientation(const glm::quat& orientation); // Sets orientation in world coordinates
    void SetLinearVelocity(const glm::vec3& velocity); // Sets linear velocity in world coordinates
    void SetAngularVelocity(const glm::vec3& velocity); // Sets angular velocity in world coordinates


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