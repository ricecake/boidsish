#include "rigid_body.h"

#include <iostream>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

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

			glm::vec3 moment = (glm::vec3(delta.dual.x, delta.dual.y, delta.dual.z) -
			                    axis * (0.5f * d * cos_half_theta)) /
				sin_half_theta;

			new_dual.w = -0.5f * new_d * std::sin(new_half_theta);
			glm::vec3 complex_dual = axis * (0.5f * new_d * std::cos(new_half_theta)) +
				moment * std::sin(new_half_theta);
			new_dual.x = complex_dual.x;
			new_dual.y = complex_dual.y;
			new_dual.z = complex_dual.z;
		}

		glm::dualquat delta_t(new_real, new_dual);
		return from * delta_t;
	}
} // namespace

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

	return "Pos: " + glm::to_string(pos) + ", Orient: " + glm::to_string(orient) +
		", LinVel: " + glm::to_string(linVel) + ", AngVel: " + glm::to_string(angVel);
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

void RigidBody::AddWrench(const glm::dualquat& wrench) {
	wrench_accumulator_.real += wrench.real;
	wrench_accumulator_.dual += wrench.dual;
}

void RigidBody::SetPersistentWrench(const glm::dualquat& wrench) {
	persistent_wrench_ = wrench;
}

void RigidBody::ClearWrench() {
	wrench_accumulator_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));
	persistent_wrench_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));
}

void RigidBody::AddForce(const glm::vec3& force) {
	wrench_accumulator_.dual += glm::quat(0, force.x, force.y, force.z);
}

void RigidBody::AddRelativeForce(const glm::vec3& force) {
	glm::vec3 world_force = pose_.real * force;
	AddForce(world_force);
}

void RigidBody::AddTorque(const glm::vec3& torque) {
	wrench_accumulator_.real += glm::quat(0, torque.x, torque.y, torque.z);
}

void RigidBody::AddRelativeTorque(const glm::vec3& torque) {
	glm::vec3 world_torque = pose_.real * torque;
	AddTorque(world_torque);
}

void RigidBody::Update(float dt) {
	// 0. High-level movement behaviors
	if (movement_mode_ == MovementMode::BANKING) {
		glm::quat q = GetOrientation();
		glm::vec3 world_ang_vel = GetAngularVelocity();
		glm::vec3 local_ang_vel = glm::inverse(q) * world_ang_vel;

		// Target roll proportional to yaw rate
		float yaw_rate = local_ang_vel.y;
		float target_roll = -yaw_rate * banking_amount_;
		target_roll = glm::clamp(target_roll, -max_banking_angle_, max_banking_angle_);

		// Current roll (angle between local Y and world UP projected into local space)
		glm::vec3 world_up_in_local = glm::inverse(q) * glm::vec3(0, 1, 0);

		// Roll is approximately the angle in the local XY plane
		float current_roll = atan2(-world_up_in_local.x, world_up_in_local.y);

		// PD Controller for roll torque
		float roll_error = target_roll - current_roll;
		float roll_torque = roll_error * banking_kp_ - local_ang_vel.z * banking_kd_;

		AddRelativeTorque(glm::vec3(0, 0, roll_torque));
	}

	// 1. Extract Force and Torque from Wrench
	glm::dualquat total_wrench = persistent_wrench_ + wrench_accumulator_;
	glm::vec3     force = glm::vec3(total_wrench.dual.x, total_wrench.dual.y, total_wrench.dual.z);
	glm::vec3     torque = glm::vec3(total_wrench.real.x, total_wrench.real.y, total_wrench.real.z);

	// Apply Torque Limit
	if (max_torque_ > 0.0f) {
		float torque_len = glm::length(torque);
		if (torque_len > max_torque_) {
			torque = (torque / torque_len) * max_torque_;
		}
	}

	// 2. Update Velocities (Semi-Implicit Euler)
	glm::vec3 linear_accel = force / mass_;
	glm::vec3 linear_vel = GetLinearVelocity() + linear_accel * dt;

	glm::vec3 angular_accel = torque / inertia_;
	glm::vec3 angular_vel = GetAngularVelocity() + angular_accel * dt;

	// 3. Apply Damping (Friction)
	float lin_damping = glm::clamp(1.0f - (linear_friction_ * dt), 0.0f, 1.0f);
	float ang_damping = glm::clamp(1.0f - (angular_friction_ * dt), 0.0f, 1.0f);

	linear_vel *= lin_damping;
	angular_vel *= ang_damping;

	// 4. Apply Velocity Limits
	if (max_linear_velocity_ > 0.0f) {
		float speed = glm::length(linear_vel);
		if (speed > max_linear_velocity_) {
			linear_vel = (linear_vel / speed) * max_linear_velocity_;
		}
	}
	if (max_angular_velocity_ > 0.0f) {
		float ang_speed = glm::length(angular_vel);
		if (ang_speed > max_angular_velocity_) {
			angular_vel = (angular_vel / ang_speed) * max_angular_velocity_;
		}
	}

	// 5. Update Position
	glm::vec3 current_pos = GetPosition();
	glm::vec3 new_pos = current_pos + linear_vel * dt;

	// 6. Update Orientation (Quaternion Derivative)
	glm::quat current_orient = GetOrientation();
	glm::quat new_orient = current_orient;

	// Only integrate if we have significant angular velocity
	if (glm::length2(angular_vel) > 0.000001f) { // length2 avoids sqrt
		// w * q * 0.5 * dt
		glm::quat omega_q(0, angular_vel.x, angular_vel.y, angular_vel.z);
		glm::quat spin = 0.5f * omega_q * current_orient;

		new_orient = current_orient + (spin * dt);
		new_orient = glm::normalize(new_orient); // Normalize immediately
	}

	// 7. Update State
	SetLinearVelocity(linear_vel);
	SetAngularVelocity(angular_vel);

	// Reconstruct Pose
	pose_.real = new_orient;
	// P_dual = 0.5 * t * r
	pose_.dual = 0.5f * glm::quat(0, new_pos.x, new_pos.y, new_pos.z) * new_orient;

	// 8. FINAL SAFETY NORMALIZATION
	// This catches any drift from the position reconstruction
	pose_ = normalizeDualQuat(pose_);

	// Clear frame accumulator
	wrench_accumulator_ = glm::dualquat(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));
}

void RigidBody::FaceVelocity() {
	glm::vec3 linVel = glm::vec3(twist_.dual.x, twist_.dual.y, twist_.dual.z);

	if (glm::length(linVel) > 0.001f) {
		glm::vec3 direction = glm::normalize(linVel);
		glm::quat targetRot = glm::quatLookAt(direction, glm::vec3(0, 1, 0.01f));
		glm::vec3 currentPos = GetTranslation(pose_);
		pose_ = glm::dualquat(targetRot, currentPos);
	}
}
