#include "wind_field.h"

#include "Simplex.h"
#include "terrain_generator_interface.h"
#include <algorithm>
#include <tuple>
#include <glm/gtx/norm.hpp>

namespace Boidsish {

	WindField::WindField(std::shared_ptr<ITerrainGenerator> terrain): terrain_(terrain) {}

	glm::vec3 WindField::Sample(const glm::vec3& pos, float time) const {
		if (!terrain_)
			return glm::vec3(0.0f);

		// 1. BASE CURL NOISE (Low frequency)
		// We sample 3D curl noise to get a divergence-free base field.
		glm::vec3 noise_pos = pos * noise_scale_;
		glm::vec3 base_wind = Simplex::curlNoise(noise_pos + glm::vec3(0.0f, time * 0.1f, 0.0f)) * base_speed_;

		// 2. TERRAIN AWARENESS
		float     height;
		glm::vec3 normal;
		std::tie(height, normal) = terrain_->GetTerrainPropertiesAtPoint(pos.x, pos.z);
		float dist_above = pos.y - height;

		// 3. VALLEY FLOW
		// Canyons/valleys are where the "path" is. GetPathData returns (dist_from_spine, dx, dz).
		glm::vec3 path_data = terrain_->GetPathData(pos.x, pos.z);
		float     valley_factor = 1.0f - glm::smoothstep(0.0f, 40.0f, std::abs(path_data.x));

		// In valleys, we want the wind to follow the valley direction (tangent to the spine).
		// The path gradient is (path_data.y, path_data.z). The tangent is (-path_data.z, path_data.y).
		glm::vec3 valley_dir = glm::vec3(-path_data.z, 0.0f, path_data.y);
		if (glm::length2(valley_dir) > 1e-6f) {
			valley_dir = glm::normalize(valley_dir);
		} else {
			valley_dir = glm::vec3(0.0f);
		}

		// Check if we should reverse valley_dir to align with base wind
		if (glm::dot(valley_dir, base_wind) < 0.0f) {
			valley_dir = -valley_dir;
		}

		// Blend in valley flow
		base_wind = glm::mix(base_wind, valley_dir * base_speed_ * valley_strength_, valley_factor * 0.7f);

		// 4. UPDRAFTS IN OPEN SPACES
		// If we are far from the valley and have low slope, we add an updraft.
		float slope = 1.0f - normal.y; // 0 is flat, 1 is vertical
		float open_space_factor = (1.0f - valley_factor) * (1.0f - glm::smoothstep(0.0f, 0.5f, slope));
		base_wind.y += open_space_factor * updraft_strength_ * base_speed_;

		// 5. CONSERVATIVE GROUND AVOIDANCE
		// To make it "conservative" and avoid hitting the ground, we can project the wind
		// onto the plane of the terrain if we are close to the ground.
		if (dist_above < ground_avoidance_dist_) {
			float     t = 1.0f - glm::smoothstep(0.0f, ground_avoidance_dist_, dist_above);
			glm::vec3 projected = base_wind - glm::dot(base_wind, normal) * normal;

			// Add a slight upward bias near ground to ensure it stays "adrift"
			glm::vec3 lift = normal * base_speed_ * 0.5f;

			base_wind = glm::mix(base_wind, projected + lift, t);
		}

		// Stronger push if we are actually below ground
		if (dist_above < 0.0f) {
			base_wind += normal * std::abs(dist_above) * 2.0f;
		}

		return base_wind;
	}

} // namespace Boidsish
