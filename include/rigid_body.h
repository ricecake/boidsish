#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

enum class MovementMode {
	DEFAULT = 0,
	BANKING = 1
};

class RigidBody {
public:
	// Physical properties
	float     mass_ = 1.0f;
	glm::vec3 inertia_ = glm::vec3(1.0f, 1.0f, 1.0f);
	float     linear_friction_ = 7.5f;
	float     angular_friction_ = 7.5f;

private:
	// Position + Orientation (Pose)
	glm::dualquat pose_;

	// Linear + Angular Velocity (Twist)
	glm::dualquat twist_;

	// Wrench tracking (Persistent + Accumulated)
	glm::dualquat persistent_wrench_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));
	glm::dualquat wrench_accumulator_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));

	// Limits
	float max_linear_velocity_ = -1.0f;
	float max_angular_velocity_ = -1.0f;
	float max_torque_ = -1.0f;

	// Movement mode and banking
	MovementMode movement_mode_ = MovementMode::DEFAULT;
	float        banking_amount_ = 1.5f;
	float        max_banking_angle_ = 0.785f; // 45 degrees in radians
	float        banking_kp_ = 100.0f;
	float        banking_kd_ = 10.0f;

public:
	// Constructors
	RigidBody(): pose_(glm::quat(1, 0, 0, 0), glm::vec3(0)), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position):
		pose_(glm::quat(1, 0, 0, 0), position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};
	RigidBody(const glm::vec3& position, const glm::quat& orientation):
		pose_(orientation, position), twist_(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};

	// Getters
	glm::vec3   GetPosition() const;
	glm::quat   GetOrientation() const;
	glm::vec3   GetLinearVelocity() const;
	glm::vec3   GetAngularVelocity() const;
	std::string ToString() const;

	// Setters
	void SetPosition(const glm::vec3& position);
	void SetOrientation(const glm::quat& orientation);
	void SetLinearVelocity(const glm::vec3& velocity);
	void SetAngularVelocity(const glm::vec3& velocity);

	// Wrench Application
	void AddWrench(const glm::dualquat& wrench);
	void SetPersistentWrench(const glm::dualquat& wrench);
	void ClearWrench();

	// Force and Torque Application (Traditional)
	void AddForce(const glm::vec3& force);         // Add force in world coordinates
	void AddRelativeForce(const glm::vec3& force); // Add force in local coordinates

	void AddTorque(const glm::vec3& torque);         // Add torque in world coordinates
	void AddRelativeTorque(const glm::vec3& torque); // Add torque in local coordinates

	// Limits
	void SetMaxLinearVelocity(float v) { max_linear_velocity_ = v; }
	void SetMaxAngularVelocity(float v) { max_angular_velocity_ = v; }
	void SetMaxTorque(float t) { max_torque_ = t; }

	// Movement Mode
	void         SetMovementMode(MovementMode mode) { movement_mode_ = mode; }
	MovementMode GetMovementMode() const { return movement_mode_; }

	// Banking Parameters
	void SetBankingAmount(float amount) { banking_amount_ = amount; }
	void SetMaxBankingAngle(float radians) { max_banking_angle_ = radians; }
	void SetBankingPD(float kp, float kd) {
		banking_kp_ = kp;
		banking_kd_ = kd;
	}

	// Main integration step
	void Update(float dt);

	// Utility
	void FaceVelocity();
};