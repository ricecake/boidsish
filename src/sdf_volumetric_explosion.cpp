#include "sdf_volumetric_explosion.h"
#include "sdf_volume_manager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Boidsish {

	SdfVolumetricExplosion::SdfVolumetricExplosion(
		SdfVolumeManager* manager,
		const glm::vec3&  position,
		float             max_radius,
		float             duration
	):
		Shape(0, position.x, position.y, position.z),
		manager_(manager),
		max_radius_(max_radius),
		duration_(duration) {

		SetHidden(true);

		SdfSource source;
		source.position = position;
		source.radius = 0.0f;
		source.color = glm::vec3(1.0f, 0.9f, 0.8f);
		source.smoothness = 1.0f;
		source.charge = 1.0f;
		source.type = 2; // TYPE_VOLUMETRIC
		source.noise_intensity = 0.0f;
		source.noise_scale = 1.0f;

		source_id_ = manager_->AddSource(source);
	}

	SdfVolumetricExplosion::~SdfVolumetricExplosion() {
		manager_->RemoveSource(source_id_);
	}

	void SdfVolumetricExplosion::Update(float delta_time) {
		time_lived_ += delta_time;
		float f = std::clamp(time_lived_ / duration_, 0.0f, 1.0f);

		float current_radius;
		float noise_intensity;

		if (f < 0.2f) {
			current_radius = max_radius_ * (f / 0.2f);
			noise_intensity = 1.0f * (f / 0.2f);
		} else {
			current_radius = max_radius_ * (1.0f + 0.2f * (f - 0.2f) / 0.8f);
			noise_intensity = 1.0f * (1.0f - (f - 0.2f) / 0.8f);
		}

		SdfSource source;
		source.position = glm::vec3(GetX(), GetY(), GetZ());
		source.radius = current_radius;
		source.color = glm::vec3(1.0f, 0.9f, 0.8f);
		source.smoothness = 1.0f;
		source.charge = 1.0f;
		source.type = 2; // TYPE_VOLUMETRIC
		source.noise_intensity = noise_intensity;
		source.noise_scale = 0.5f;

		manager_->UpdateSource(source_id_, source);
	}

	glm::mat4 SdfVolumetricExplosion::GetModelMatrix() const {
		return glm::translate(glm::mat4(1.0f), glm::vec3(GetX(), GetY(), GetZ()));
	}

	bool SdfVolumetricExplosion::IsExpired() const {
		return time_lived_ >= duration_;
	}

} // namespace Boidsish
