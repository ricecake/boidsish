#include "ShieldBoid.h"
#include "polyhedron.h"
#include "spatial_entity_handler.h"
#include "graphics.h"
#include "GuidedMissile.h"
#include "Swooper.h"
#include "Potshot.h"
#include "CongaMarcher.h"
#include "KittywumpusPlane.h"
#include "RedDotEnemy.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	ShieldBoid::ShieldBoid(int id, const glm::vec3& orbit_point) : Entity<Polyhedron>(id, PolyhedronType::Icosahedron), orbit_point_(orbit_point) {
		SetPosition(orbit_point.x, orbit_point.y, orbit_point.z);
		SetSize(2.0f);
		SetTrailLength(40);
		SetTrailIridescence(true);
		SetColor(0.2f, 0.8f, 1.0f); // Bright cyan/blue
		SetUsePBR(true);
		SetMetallic(1.0f);
		SetRoughness(0.1f);

		orbit_angle_ = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f * glm::pi<float>();
		orbit_radius_ = 10.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 5.0f;
		vertical_offset_ = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f - 5.0f;
	}

	void ShieldBoid::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		switch (state_) {
			case ShieldBoidState::WILD:
				UpdateWild(handler, delta_time);
				break;
			case ShieldBoidState::CAPTURED:
				UpdateCaptured(handler, time, delta_time);
				break;
			case ShieldBoidState::INTERCEPTING:
				UpdateIntercepting(handler, delta_time);
				break;
		}
		UpdateShape();
	}

	void ShieldBoid::SetTarget(std::shared_ptr<EntityBase> target) {
		target_ = target;
	}

	void ShieldBoid::SetOrbitPoint(const glm::vec3& point) {
		orbit_point_ = point;
	}

	void ShieldBoid::Capture(std::shared_ptr<EntityBase> player) {
		target_ = player;
		state_ = ShieldBoidState::CAPTURED;
		SetColor(1.0f, 0.9f, 0.2f); // Golden when captured
	}

	void ShieldBoid::UpdateWild(const EntityHandler& handler, float delta_time) {
		orbit_angle_ += orbit_speed_ * delta_time;

		glm::vec3 offset(
			cos(orbit_angle_) * orbit_radius_,
			vertical_offset_ + sin(orbit_angle_ * 0.5f) * 2.0f,
			sin(orbit_angle_) * orbit_radius_
		);

		glm::vec3 target_pos = orbit_point_ + offset;
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 dir = target_pos - current_pos;
		float dist = glm::length(dir);

		if (dist > 0.1f) {
			glm::vec3 vel = glm::normalize(dir) * std::min(dist * 5.0f, 50.0f);
			SetVelocity(Vector3(vel.x, vel.y, vel.z));
		}
	}

	void ShieldBoid::UpdateCaptured(const EntityHandler& handler, float time, float delta_time) {
		if (!target_) return;

		glm::vec3 player_pos = target_->GetPosition().Toglm();

		// Check for threats
		auto threat = FindThreat(handler, player_pos);
		if (threat) {
			intercept_target_ = threat;
			state_ = ShieldBoidState::INTERCEPTING;
			SetColor(1.0f, 0.1f, 0.1f); // Red when intercepting
			return;
		}

		// Spiral orbit around player's forward axis
		glm::quat player_orient = target_->GetOrientation();
		glm::vec3 forward = player_orient * glm::vec3(0, 0, -1);
		glm::vec3 right = player_orient * glm::vec3(1, 0, 0);
		glm::vec3 up = player_orient * glm::vec3(0, 1, 0);

		orbit_angle_ += orbit_speed_ * delta_time;
		float spiral_offset = sin(time * spiral_speed_) * 5.0f;

		glm::vec3 orbit_offset = (right * cos(orbit_angle_) + up * sin(orbit_angle_)) * orbit_radius_;
		orbit_offset += forward * (spiral_offset - 10.0f); // Stay slightly behind player

		glm::vec3 target_pos = player_pos + orbit_offset;
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 dir = target_pos - current_pos;
		float dist = glm::length(dir);

		if (dist > 0.1f) {
			glm::vec3 player_vel = target_->GetVelocity().Toglm();
			glm::vec3 vel = player_vel + glm::normalize(dir) * std::min(dist * 10.0f, 100.0f);
			SetVelocity(Vector3(vel.x, vel.y, vel.z));
		}
	}

	void ShieldBoid::UpdateIntercepting(const EntityHandler& handler, float delta_time) {
		if (!intercept_target_ || !handler.GetEntity(intercept_target_->GetId())) {
			state_ = ShieldBoidState::CAPTURED;
			SetColor(1.0f, 0.9f, 0.2f);
			intercept_target_ = nullptr;
			return;
		}

		glm::vec3 target_pos = intercept_target_->GetPosition().Toglm();
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 dir = target_pos - current_pos;
		float dist = glm::length(dir);

		if (dist < 5.0f) {
			// Impact!
			intercept_target_->OnHit(handler, 100.0f); // Destroy target

			handler.EnqueueVisualizerAction([current_pos, vis = handler.vis]() {
				if (vis) vis->AddFireEffect(current_pos, FireEffectStyle::Explosion, glm::vec3(0, 1, 0), glm::vec3(0, 0, 0), -1, 0.5f);
			});

			handler.QueueRemoveEntity(id_);
			return;
		}

		glm::vec3 vel = glm::normalize(dir) * 200.0f; // Dash to target
		SetVelocity(Vector3(vel.x, vel.y, vel.z));
	}

	std::shared_ptr<EntityBase> ShieldBoid::FindThreat(const EntityHandler& handler, const glm::vec3& player_pos) {
		const auto& entities = handler.GetAllEntities();
		for (auto const& [id, entity] : entities) {
			if (entity->IsThreat()) {
				if (glm::distance(entity->GetPosition().Toglm(), player_pos) < 100.0f) {
					return entity;
				}
			}
		}
		return nullptr;
	}

} // namespace Boidsish
