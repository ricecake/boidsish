#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Boidsish {

	class ITerrainGenerator;

	/**
	 * @brief Procedural wind field generator that is terrain-aware and conservative.
	 *
	 * Uses curl noise to ensure a divergence-free (conservative) flow field.
	 * The potential field is modulated by terrain distance to ensure the wind
	 * flows parallel to the surface near the ground, preventing collisions.
	 */
	class WindField {
	public:
		WindField(std::shared_ptr<ITerrainGenerator> terrain);

		/**
		 * @brief Sample the wind velocity at a given 3D position and time.
		 * @param pos The 3D world position to sample at.
		 * @param time The current time for procedural animation.
		 * @return The wind velocity vector at that point.
		 */
		glm::vec3 Sample(const glm::vec3& pos, float time) const;

		// Configuration
		void SetBaseSpeed(float s) { base_speed_ = s; }

		void SetNoiseScale(float s) { noise_scale_ = s; }

		void SetValleyStrength(float s) { valley_strength_ = s; }

		void SetUpdraftStrength(float s) { updraft_strength_ = s; }

	private:
		std::shared_ptr<ITerrainGenerator> terrain_;

		float base_speed_ = 10.0f;
		float noise_scale_ = 0.005f;     // Very low frequency as requested
		float valley_strength_ = 2.0f;   // Multiplier in canyons/valleys
		float updraft_strength_ = 0.5f;  // Updraft bias in open spaces
		float ground_avoidance_dist_ = 20.0f;
	};

} // namespace Boidsish
