#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Boidsish {

	class Model;

	enum class FireEffectStyle { MissileExhaust, Explosion, Fire, Sparks, Glitter, Ambient, Bubbles, Fireflies, Null };

	enum class EmitterType { Point = 0, Box = 1, Sphere = 2, Beam = 3, Model = 4 };

	class FireEffect {
	public:
		FireEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f,
			EmitterType      type = EmitterType::Point,
			const glm::vec3& dimensions = glm::vec3(0.0f),
			float            sweep = 1.0f
		);

		void SetPosition(const glm::vec3& pos) { position_ = pos; }

		void SetStyle(FireEffectStyle style) { style_ = style; }

		void SetDirection(const glm::vec3& dir) { direction_ = dir; }

		void SetVelocity(const glm::vec3& vel) { velocity_ = vel; }

		void SetDimensions(const glm::vec3& dim) { dimensions_ = dim; }

		void SetType(EmitterType type) { type_ = type; }

		void SetSweep(float sweep) { sweep_ = sweep; }

		void SetActive(bool active) { active_ = active; }

		const glm::vec3& GetPosition() const { return position_; }

		FireEffectStyle GetStyle() const { return style_; }

		const glm::vec3& GetDirection() const { return direction_; }

		const glm::vec3& GetVelocity() const { return velocity_; }

		const glm::vec3& GetDimensions() const { return dimensions_; }

		EmitterType GetType() const { return type_; }

		float GetSweep() const { return sweep_; }

		void SetSourceModel(std::shared_ptr<Model> model) { source_model_ = model; }

		std::shared_ptr<Model> GetSourceModel() const { return source_model_.lock(); }

		int GetId() const { return id_; }

		int GetMaxParticles() const { return max_particles_; }

		bool IsActive() const { return active_; }

		float GetLifetime() const { return lifetime_; }

		void SetLifetime(float lifetime) { lifetime_ = lifetime; }

		float GetLived() const { return lived_; }

		void SetLived(float lived) { lived_ = lived; }

	private:
		inline static int count = 1;
		glm::vec3         position_;
		FireEffectStyle   style_;
		glm::vec3         direction_;
		int               id_;
		glm::vec3         velocity_;
		int               max_particles_;
		bool              active_{true};
		float             lifetime_ = -1.0f;
		float             lived_ = 0.0f;
		glm::vec3              dimensions_;
		EmitterType            type_;
		float                  sweep_ = 1.0f;
		std::weak_ptr<Model>   source_model_;
	};

} // namespace Boidsish
