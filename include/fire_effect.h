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
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f,
			bool             needs_terrain_data = false
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

		int GetMaxParticles() const { return max_particles_; }

		bool IsActive() const { return active_; }

		float GetLifetime() const { return lifetime_; }

		void SetLifetime(float lifetime) { lifetime_ = lifetime; }

		float GetLived() const { return lived_; }

		void SetLived(float lived) { lived_ = lived; }

		bool GetNeedsTerrainData() const { return needs_terrain_data_; }

		int GetTerrainTextureLayer() const { return terrain_texture_layer_; }

		void SetTerrainTextureLayer(int layer) { terrain_texture_layer_ = layer; }

		const glm::vec3& GetTerrainTextureOrigin() const { return terrain_texture_origin_; }

		void SetTerrainTextureOrigin(const glm::vec3& origin) { terrain_texture_origin_ = origin; }

		const glm::vec3& GetLastTerrainQueryCenter() const { return last_terrain_query_center_; }

		void SetLastTerrainQueryCenter(const glm::vec3& center) { last_terrain_query_center_ = center; }

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
		bool              needs_terrain_data_{false};
		int               terrain_texture_layer_{-1};
		glm::vec3         terrain_texture_origin_{0.0f};
		glm::vec3         last_terrain_query_center_{0.0f};
	};

} // namespace Boidsish
