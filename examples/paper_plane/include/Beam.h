#pragma once

#include "entity.h"
#include "line.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Beam: public Entity<Line> {
	public:
		enum class State { IDLE, AIMING, FIRING_TRANSITION, FIRING_HOLD, FIRING_SHRINK, COOLDOWN };

		Beam(int id, int owner_id);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

		void SetRequesting(bool requesting);
		void SetOffset(const glm::vec3& offset) { offset_ = offset; }
		void SetRelativeDirection(const glm::vec3& dir) { relative_dir_ = glm::normalize(dir); }

		State GetState() const { return state_; }
		int   GetOwnerId() const { return owner_id_; }

	private:
		int   owner_id_;
		State state_ = State::IDLE;
		float state_timer_ = 0.0f;
		bool  requesting_ = false;

		glm::vec3 offset_ = glm::vec3(0.0f);
		glm::vec3 relative_dir_ = glm::vec3(0.0f, 0.0f, -1.0f);

		// Configuration
		static constexpr float kTransitionDuration = 0.5f;
		static constexpr float kHoldDuration = 0.3f;
		static constexpr float kShrinkDuration = 0.4f;
		static constexpr float kCooldownDuration = 0.5f;

		static constexpr float kAimingWidth = 2.0f;
		static constexpr float kFiringWidth = 0.4f;
		static constexpr float kShrinkWidth = 0.05f;

		static constexpr float kDamageRadius = 25.0f;
	};

} // namespace Boidsish
