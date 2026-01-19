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

    glm::dualquat conjugate(const glm::dualquat& dq) {
        return glm::dualquat(glm::conjugate(dq.real), glm::conjugate(dq.dual));
    }

    glm::dualquat normalizeDualQuat(const glm::dualquat& dq) {
        float magnitude = glm::length(dq.real);

        // If the magnitude is too small, return identity to avoid division by zero
        if (magnitude < 0.000001f) {
            return glm::dualquat(glm::quat(1, 0, 0, 0), glm::vec3(0));
        }

        float invMag = 1.0f / magnitude;

        glm::quat r = dq.real * invMag;
        glm::quat d = (dq.dual - r * glm::dot(r, dq.dual)) * invMag;

        return glm::dualquat(r, d);
    }

    // Helper to convert a vec3 into a "point" dual quaternion (1 + εv)
    glm::dualquat pointToDQ(const glm::vec3& v) {
        return glm::dualquat(glm::quat(1, 0, 0, 0), glm::quat(0, v.x, v.y, v.z));
    }

    // Helper to extract the vec3 from a "point" dual quaternion
    glm::vec3 dqToPoint(const glm::dualquat& dq) {
        return glm::vec3(dq.dual.x, dq.dual.y, dq.dual.z);
    }

    glm::dualquat getInverse(const glm::dualquat& dq) {
        return conjugate(dq);
    }

    glm::vec3 transformPoint(const glm::dualquat& pose, const glm::vec3& point) {
        glm::dualquat p = pointToDQ(point);

        glm::dualquat invPose = getInverse(pose);
        glm::dualquat result = pose * p * invPose;

        return dqToPoint(result);
    }

    glm::quat GetRotation(glm::dualquat& dq) {
        return dq.real;
    }

    glm::vec3 GetTranslation(glm::dualquat& dq) {
        glm::quat dual_part = dq.dual;
        glm::quat real_part = dq.real;
        glm::quat t_quat = (2.0f * dual_part) * glm::conjugate(real_part);
        return glm::vec3(t_quat.x, t_quat.y, t_quat.z);
    }

    // 1. Local-to-World (Transforming a point into the world)
    glm::vec3 LocalToWorld(const glm::dualquat& pose, const glm::vec3& localPoint) {
        glm::dualquat p = pointToDQ(localPoint);
        glm::dualquat transformed = pose * p * conjugate(pose);
        return dqToPoint(transformed);
    }

    glm::vec3 WorldToLocal(const glm::dualquat& pose, const glm::vec3& worldPoint) {
        glm::dualquat invPose = conjugate(pose);
        glm::dualquat p(glm::quat(1, 0, 0, 0), glm::quat(0, worldPoint.x, worldPoint.y, worldPoint.z));
        glm::dualquat invPoseConj = conjugate(invPose);
        glm::dualquat transformed = invPose * p * invPoseConj;

        return glm::vec3(transformed.dual.x, transformed.dual.y, transformed.dual.z);
    }

    glm::dualquat dq_pow(const glm::dualquat& dq, float exponent);

    glm::dualquat dq_log(const glm::dualquat& dq) {
        glm::quat q_rot = dq.real;
        float     cos_theta = q_rot.w;

        // Safety for arccos domain
        if (cos_theta > 1.0f)
            cos_theta = 1.0f;
        if (cos_theta < -1.0f)
            cos_theta = -1.0f;

        float theta = 2.0f * std::acos(cos_theta);
        float sin_half_theta = std::sqrt(1.0f - cos_theta * cos_theta);

        glm::vec3 rot_axis;
        float     scalar_r; // Used to scale the dual part

        if (std::abs(sin_half_theta) < 0.001f) {
            rot_axis = glm::vec3(q_rot.x, q_rot.y, q_rot.z);
            scalar_r = 1.0f;
        } else {
            rot_axis = glm::vec3(q_rot.x, q_rot.y, q_rot.z) / sin_half_theta;
            theta = (cos_theta < 0) ? (theta - 6.2831853f) : theta; // Shortest path
            scalar_r = (theta * 0.5f) / sin_half_theta;             // (theta/2) / sin(theta/2)
        }

        glm::quat q_trans = dq.dual;
        glm::quat real_log = glm::quat(0, rot_axis * (theta * 0.5f));


        return glm::dualquat(real_log, glm::quat(0, 0, 0, 0)); // Placeholder if we don't do full Screw Log
    }

    // SIMPLIFIED ScLERP (Screw Linear Interpolation)
    // This implements the exact Screw Path without needing the full Log/Exp math suite.
    // It decomposes the motion into Angle (theta) and Pitch (d).
    glm::dualquat ScLERP(const glm::dualquat& from, const glm::dualquat& to, float t) {
        glm::dualquat delta = conjugate(from) * to;

        // 2. Ensure shortest path (flip sign if dot < 0)
        if (glm::dot(from.real, to.real) < 0.0f) {
            delta = -delta;
        }

        float cos_half_theta = delta.real.w;
        float half_theta = std::acos(glm::clamp(cos_half_theta, -1.0f, 1.0f));
        float sin_half_theta = std::sqrt(1.0f - cos_half_theta * cos_half_theta);

        float new_half_theta = half_theta * t;

        glm::quat new_real;
        glm::quat new_dual;

        if (sin_half_theta < 0.001f) {
            new_real = glm::quat(1, 0, 0, 0);
            new_dual = delta.dual * t;
        } else {
            glm::vec3 axis = glm::vec3(delta.real.x, delta.real.y, delta.real.z) / sin_half_theta;

            float d = -2.0f * delta.dual.w / sin_half_theta;
            float new_d = d * t;

            new_real = glm::quat(std::cos(new_half_theta), axis * std::sin(new_half_theta));

            glm::vec3 moment = (glm::vec3(delta.dual.x, delta.dual.y, delta.dual.z) - axis * (0.5f * d * cos_half_theta)) /
                sin_half_theta;

            new_dual.w = -0.5f * new_d * std::sin(new_half_theta);
            glm::vec3 complex_dual = axis * (0.5f * new_d * std::cos(new_half_theta)) + moment * std::sin(new_half_theta);
            new_dual.x = complex_dual.x;
            new_dual.y = complex_dual.y;
            new_dual.z = complex_dual.z;
        }

        glm::dualquat delta_t(new_real, new_dual);
        return from * delta_t;
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
    float linear_damping = 1.0f - (linear_friction_ * dt);
    float angular_damping = 1.0f - (angular_friction_ * dt);
    new_linear_velocity *= (linear_damping > 0.0f ? linear_damping : 0.0f);
    new_angular_velocity *= (angular_damping > 0.0f ? angular_damping : 0.0f);

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

// void RigidBody::FaceVelocity() {
	void RigidBody::FaceVelocity() {
		glm::vec3 linVel = glm::vec3(twist_.dual.x, twist_.dual.y, twist_.dual.z);

		if (glm::length(linVel) > 0.001f) {
			glm::vec3 direction = glm::normalize(linVel);
			glm::quat targetRot = glm::quatLookAt(direction, glm::vec3(0, 1, 0.01f));
			glm::vec3 currentPos = GetTranslation(pose_);
			pose_ = glm::dualquat(targetRot, currentPos);
		}
	}


//     glm::vec3 linVel = GetLinearVelocity();
//     if (glm::length(linVel) > 0.001f) {
//         glm::vec3 direction = glm::normalize(linVel);
//         glm::quat targetRot = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
//         SetOrientation(targetRot);
//     }
// }

/*

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
*/

/*
Collect Inputs: Calculate Wrenches (Player input, Gravity, Bungees).
Update Velocity: twist += wrench * dt.
Update Pose: pose += (twist * pose) * (dt * 0.5f).
Clean Up: pose = normalizeDualQuat(pose).
Render: draw(glm::mat4_cast(pose)).
*/
/*

class RigidBody {
private:
	// Position + Orientation
	// Must be a unit dual quaternion.
	glm::dualquat pose;

	// Linear + Angular Velocity
	// Represents the rate of change.
	// Real scalar and Dual scalar are typically 0.
	glm::dualquat twist;

public:
	RigidBody(): pose(glm::quat(1, 0, 0, 0), glm::vec3(0)), twist(glm::quat(0, 0, 0, 0), glm::vec3(0)) {};

	RigidBody(glm::quat orientation, glm::quat rotation):
		pose(orientation, glm::vec3(0)), twist(rotation, glm::vec3(0)) {};

	RigidBody(glm::quat orientation, glm::quat rotation, glm::vec3 position):
		pose(orientation, position), twist(rotation, glm::vec3(0)) {};

	RigidBody(glm::vec3 position, glm::vec3 velocity):
		pose(glm::quat(1, 0, 0, 0), position), twist(glm::quat(1, 0, 0, 0), velocity) {};

	RigidBody(glm::vec3 position, glm::vec3 velocity, glm::quat orientation):
		pose(orientation, position), twist(glm::quat(0, 0, 0, 0), velocity) {};

	RigidBody(glm::vec3 position, glm::quat orientation, glm::vec3 velocity, glm::quat rotation):
		pose(orientation, position), twist(rotation, velocity) {};

	void FaceVelocity() {
		glm::vec3 linVel = glm::vec3(twist.dual.x, twist.dual.y, twist.dual.z);

		if (glm::length(linVel) > 0.001f) {
			glm::vec3 direction = glm::normalize(linVel);
			glm::quat targetRot = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
			glm::vec3 currentPos = GetTranslation(pose);
			pose = glm::dualquat(targetRot, currentPos);
		}
	}

	void SmoothishPathToTarget(const glm::dualquat& targetPose, float alpha) {
		pose = glm::lerp(pose, targetPose, alpha);
		pose = normalizeDualQuat(pose);
	}

	void SmootherPathToTarget(const glm::dualquat& targetPose, float alpha) {
		float dot = glm::dot(pose.real, targetPose.real);

		glm::dualquat target = targetPose;
		if (dot < 0.0f) {
			target = -targetPose;
		}

		// pose = glm::mix(pose, target, alpha);
		pose.real = glm::mix(pose.real, target.real, alpha);
		pose.dual = glm::mix(pose.dual, target.dual, alpha);
		pose = normalizeDualQuat(pose);
	}

	void ApplyForwardAcceleration(float force, float dt) {
		// // IF twist is World Space (Standard Physics):
		// pose += (twist * pose) * (dt * 0.5f);

		// // IF twist is Local Space (Arcade/Car physics):
		// pose += (pose * twist) * (dt * 0.5f);
		glm::vec3 forward = pose.real * glm::vec3(0, 0, -1);
		glm::vec3 linearAccel = forward * force * dt;
		twist.dual += glm::quat(0, linearAccel.x, linearAccel.y, linearAccel.z);
	}

	void Update(float dt) {
		// FIX 2: Assuming Twist is World Space (based on your ApplyForwardAccel)
		// Order: Twist * Pose
		pose = pose + (twist * pose) * (dt * 0.5f);

		pose = normalizeDualQuat(pose);
	}
};
*/

/*

// 1. Define a local line (Direction = Z-forward, passing through origin)
// Direction = (0,0,-1), Moment = (0,0,0) because it passes through origin
glm::dualquat localLine(glm::quat(0, 0, 0, -1), glm::quat(0, 0, 0, 0));

// 2. Transform the line to World Space
// WorldLine = Pose * LocalLine * Conjugate(Pose)
glm::dualquat worldLine = actor.pose * localLine * glm::conjugate(actor.pose);

// 3. Extract World Data
glm::vec3 worldDir = glm::vec3(worldLine.real.x, worldLine.real.y, worldLine.real.z);
glm::vec3 worldMom = glm::vec3(worldLine.dual.x, worldLine.dual.y, worldLine.dual.z);

glm::vec3 closestPoint = glm::cross(worldDir, worldMom);

// 1. Construction
// Representing: "Facing North (Identity) and moving 10 units up"
glm::quat rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
glm::vec3 translation = glm::vec3(0, 10, 0);

// GLM constructor: dualquat(rotation, translation)
// Internally this computes: q_real = rotation, q_dual = 0.5 * translation_quat * rotation
glm::dualquat dq_delta = glm::dualquat(rotation, translation);

// 2. Temporal Application (Motion over time T)
// Assume t ranges from 0.0 to 1.0
float         t = 0.5f;
glm::dualquat identity = glm::dualquat(glm::quat(1, 0, 0, 0), glm::vec3(0));

// ScLERP (Screw Linear Interpolation) provides the smooth path for both R and T
glm::dualquat dq_interpolated = glm::slerp(identity, dq_delta, t);

// Apply to current state: NewState = Delta * CurrentState
// (Note: Order of multiplication matters, similar to matrices)
glm::dualquat current_state = glm::dual_quat_identity();
glm::dualquat next_frame = dq_interpolated * current_state;

// 3. Extraction
// To get the rotation back out:
glm::quat recovered_rot = next_frame.real;

// To get the translation back out:
// Math: t = 2 * dual * conjugate(real)
glm::quat dual_part = next_frame.dual;
glm::quat real_part = next_frame.real;
glm::quat t_quat = (2.0f * dual_part) * glm::conjugate(real_part);
glm::vec3 recovered_trans = glm::vec3(t_quat.x, t_quat.y, t_quat.z);

// 1. Current State
glm::dualquat current_pose = glm::dual_quat_identity();         // Your object's current position/rotation
glm::vec3     angular_vel = {0, 1.0f, 0}; // Rotating at 1 rad/s around Y
glm::vec3     linear_vel = {5.0f, 0, 0};  // Moving at 5 units/s along X

// 2. Create the Twist (as a non-unit dual quaternion)
// Note: GLM doesn't have a 'Twist' type, so we use dualquat with w=0
glm::quat     real_vel = glm::quat(0, angular_vel.x, angular_vel.y, angular_vel.z);
glm::quat     dual_vel = glm::quat(0, linear_vel.x, linear_vel.y, linear_vel.z);
glm::dualquat twist(real_vel, dual_vel);

// 3. Integrate over time DeltaT
float dt = 0.016f; // 60 FPS
// NewPose = CurrentPose + (dt/2 * CurrentPose * Twist)
current_pose += (current_pose * twist) * (dt * 0.5f);

// 4. Re-normalize to fix floating point drift
current_pose = glm::normalize(current_pose);

// Applying a 'kick' to the object
glm::vec3 impulse_linear = {0, 10.0f, 0};
linear_vel += impulse_linear;

// Re-construct the twist with the new velocity
twist.dual = glm::quat(0, linear_vel.x, linear_vel.y, linear_vel.z);

*/