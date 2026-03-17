#include "sdf_explosion.h"
#include "sdf_volume_manager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Boidsish {

	SdfExplosion::SdfExplosion(
		SdfVolumeManager* manager,
		const glm::vec3&  position,
		float             max_radius,
		float             duration
	):
		Shape(0, position.x, position.y, position.z),
		manager_(manager),
		max_radius_(max_radius),
		duration_(duration) {

		orbit_speeds_ = glm::vec3(4.3f, 3.7f, 5.1f);
		distance_rates_ = glm::vec3(2.1f, 2.7f, 1.9f);

		SdfSource source;
		source.position = position;
		source.radius = 0.0f;
		source.color = glm::vec3(1.0f, 0.5f, 0.0f);
		source.smoothness = max_radius * 0.4f;
		source.charge = 1.0f;
		source.type = 1; // TYPE_EXPLOSION
		source.noise_intensity = 0.0f;
		source.noise_scale = 1.0f / (max_radius * 0.5f);

		for (int i = 0; i < 3; ++i) {
			source_ids_[i] = manager_->AddSource(source);
		}
	}

	SdfExplosion::~SdfExplosion() {
		for (int i = 0; i < 3; ++i) {
			manager_->RemoveSource(source_ids_[i]);
		}
	}

	void SdfExplosion::Update(float delta_time) {
		time_lived_ += delta_time;
		float f = std::clamp(time_lived_ / duration_, 0.0f, 1.0f);

		// Lifecycle logic
		if (f < 0.2f) {
			// Phase 1: Rapid expansion
			base_radius_ = max_radius_ * (f / 0.2f);
			noise_intensity_ = max_radius_ * 0.2f * (f / 0.2f);
		} else if (f < 0.5f) {
			// Phase 2: Growth and increasing noise
			float f2 = (f - 0.2f) / 0.3f;
			base_radius_ = max_radius_ * (1.0f + f2 * 0.2f);
			noise_intensity_ = max_radius_ * (0.2f + f2 * 0.6f);
		} else {
			// Phase 3: Shrinking while noise increases
			float f3 = (f - 0.5f) / 0.5f;
			base_radius_ = max_radius_ * 1.2f * (1.0f - f3);
			noise_intensity_ = max_radius_ * (0.8f + f3 * 0.7f);
		}

		glm::vec3 center(GetX(), GetY(), GetZ());
		float t = time_lived_;

		// Varying distance from center
		float d = base_radius_ * 0.8f * (1.0f + 0.3f * std::sin(t * 2.5f));

		// Orbiting spheres
		glm::vec3 pos[3];
		// Sphere 0 orbits X axis
		pos[0] = center + glm::vec3(0.0f, d * std::cos(t * orbit_speeds_.x), d * std::sin(t * orbit_speeds_.x));
		// Sphere 1 orbits Y axis
		pos[1] = center + glm::vec3(d * std::sin(t * orbit_speeds_.y), 0.0f, d * std::cos(t * orbit_speeds_.y));
		// Sphere 2 orbits Z axis
		pos[2] = center + glm::vec3(d * std::cos(t * orbit_speeds_.z), d * std::sin(t * orbit_speeds_.z), 0.0f);

		for (int i = 0; i < 3; ++i) {
			SdfSource source;
			source.position = pos[i];
			source.radius = base_radius_;
			source.color = glm::vec3(1.0f, 0.45f, 0.05f); // More orange albedo
			source.smoothness = base_radius_ * 0.6f;
			source.charge = 1.0f;
			source.type = 1; // TYPE_EXPLOSION
			source.noise_intensity = noise_intensity_;
			source.noise_scale = 1.5f / std::max(0.1f, base_radius_);

			manager_->UpdateSource(source_ids_[i], source);
		}
	}

	glm::mat4 SdfExplosion::GetModelMatrix() const {
		return glm::translate(glm::mat4(1.0f), glm::vec3(GetX(), GetY(), GetZ()));
	}

	bool SdfExplosion::IsExpired() const {
		return time_lived_ >= duration_;
	}

} // namespace Boidsish
