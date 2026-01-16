#include "rigid_body.h"
#include <glm/gtx/string_cast.hpp>
#include <iostream>

namespace {
    glm::quat GetRotation(const glm::dualquat& dq) {
        return dq.real;
    }

    glm::vec3 GetTranslation(const glm::dualquat& dq) {
        glm::quat t_quat = (dq.dual * 2.0f) * glm::conjugate(dq.real);
        return glm::vec3(t_quat.x, t_quat.y, t_quat.z);
    }
}

// RigidBody Method Implementations
glm::vec3 RigidBody::GetPosition() const {
    return GetTranslation(pose_);
}

glm::quat RigidBody::GetOrientation() const {
    return GetRotation(pose_);
}

glm::vec3 RigidBody::GetLinearVelocity() const {
    return glm::vec3(twist_.dual.x, twist_.dual.y, twist_.dual.z);
}

glm::vec3 RigidBody::GetAngularVelocity() const {
    return glm::vec3(twist_.real.x, twist_.real.y, twist_.real.z);
}

std::string RigidBody::ToString() const {
    glm::vec3 pos = GetPosition();
    glm::quat orient = GetOrientation();
    glm::vec3 linVel = GetLinearVelocity();
    glm::vec3 angVel = GetAngularVelocity();

    return "Pos: " + glm::to_string(pos) +
           ", Orient: " + glm::to_string(orient) +
           ", LinVel: " + glm::to_string(linVel) +
           ", AngVel: " + glm::to_string(angVel);
}


void RigidBody::SetPosition(const glm::vec3& position) {
    pose_.dual = 0.5f * glm::quat(0, position.x, position.y, position.z) * pose_.real;
}

void RigidBody::SetOrientation(const glm::quat& orientation) {
    glm::vec3 pos = GetPosition();
    pose_.real = orientation;
    SetPosition(pos);
}

void RigidBody::SetLinearVelocity(const glm::vec3& velocity) {
    twist_.dual = glm::quat(0, velocity.x, velocity.y, velocity.z);
}

void RigidBody::SetAngularVelocity(const glm::vec3& velocity) {
    twist_.real = glm::quat(0, velocity.x, velocity.y, velocity.z);
}

void RigidBody::AddForce(const glm::vec3& force) {
    force_accumulator_ += force;
}

void RigidBody::AddRelativeForce(const glm::vec3& force) {
    glm::vec3 world_force = pose_.real * force;
    force_accumulator_ += world_force;
}

void RigidBody::AddTorque(const glm::vec3& torque) {
    torque_accumulator_ += torque;
}

void RigidBody::AddRelativeTorque(const glm::vec3& torque) {
    glm::vec3 world_torque = pose_.real * torque;
    torque_accumulator_ += world_torque;
}

void RigidBody::Update(float dt) {
    // Store initial velocities
    glm::vec3 initial_linear_velocity = GetLinearVelocity();
    glm::vec3 initial_angular_velocity = GetAngularVelocity();

    // Integrate for new velocities
    glm::vec3 linear_acceleration = force_accumulator_ / mass_;
    glm::vec3 new_linear_velocity = initial_linear_velocity + linear_acceleration * dt;

    glm::vec3 angular_acceleration = torque_accumulator_ / inertia_;
    glm::vec3 new_angular_velocity = initial_angular_velocity + angular_acceleration * dt;

    // Apply friction to new velocities
    new_linear_velocity *= (1.0f - linear_friction_);
    new_angular_velocity *= (1.0f - angular_friction_);

    // Update position and orientation using average velocities
    glm::vec3 avg_linear_velocity = (initial_linear_velocity + new_linear_velocity) * 0.5f;
    glm::vec3 new_position = GetPosition() + avg_linear_velocity * dt;

    glm::vec3 avg_angular_velocity = (initial_angular_velocity + new_angular_velocity) * 0.5f;
    float angle = glm::length(avg_angular_velocity) * dt;
    glm::quat new_orientation;
    if (angle > 0.0001f) {
        glm::vec3 axis = glm::normalize(avg_angular_velocity);
        glm::quat delta_rotation = glm::angleAxis(angle, axis);
        new_orientation = delta_rotation * GetOrientation();
    } else {
        new_orientation = GetOrientation();
    }

    // Now, update state
    SetLinearVelocity(new_linear_velocity);
    SetAngularVelocity(new_angular_velocity);

    pose_.real = new_orientation;
    pose_.dual = 0.5f * glm::quat(0, new_position.x, new_position.y, new_position.z) * new_orientation;

    // Clear accumulators
    force_accumulator_ = glm::vec3(0.0f);
    torque_accumulator_ = glm::vec3(0.0f);
}

void RigidBody::FaceVelocity() {
    glm::vec3 linVel = GetLinearVelocity();
    if (glm::length(linVel) > 0.001f) {
        glm::vec3 direction = glm::normalize(linVel);
        glm::quat targetRot = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
        SetOrientation(targetRot);
    }
}
