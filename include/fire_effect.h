#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	enum class FireEffectStyle { MissileExhaust, Explosion, Fire, Null };

	class FireEffect {
	public:
		FireEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f)
		);

		void SetPosition(const glm::vec3& pos) { position_ = pos; }

		void SetStyle(FireEffectStyle style) { style_ = style; }

		void SetDirection(const glm::vec3& dir) { direction_ = dir; }

		void SetVelocity(const glm::vec3& vel) { velocity_ = vel; }

		void SetActive(bool active) { active_ = active; }

		const glm::vec3& GetPosition() const { return position_; }

		FireEffectStyle GetStyle() const { return style_; }

		const glm::vec3& GetDirection() const { return direction_; }

		const glm::vec3& GetVelocity() const { return velocity_; }

		const int GetId() const { return id_; }

		bool IsActive() const { return active_; }

	private:
		inline static int count = 1;
		glm::vec3         position_;
		FireEffectStyle   style_;
		glm::vec3         direction_;
		int               id_;
		glm::vec3         velocity_;
		bool              active_{true};
	};

} // namespace Boidsish