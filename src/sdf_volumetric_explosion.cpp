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



double easeOutCirc( double t ) {
    return sqrt( t );
}

double easeInOutCirc( double t ) {
    if( t < 0.5 ) {
        return (1 - sqrt( 1 - 2 * t )) * 0.5;
    } else {
        return (1 + sqrt( 2 * t - 1 )) * 0.5;
    }
}

double easeOutQuint( double t ) {
    double t2 = (--t) * t;
    return 1 + t * t2 * t2;
}


double easeInQuint( double t ) {
    double t2 = t * t;
    return t * t2 * t2;
}

auto adsr(auto attack, auto sustain, auto f) {
	return glm::smoothstep(0.0f, attack, f) * (1.0f - glm::smoothstep(sustain, 1.0f, f));
}

	void SdfVolumetricExplosion::Update(float delta_time) {
		time_lived_ += delta_time;
		auto f = std::clamp(time_lived_ / duration_, 0.0f, 1.0f);

		float current_radius;
		float noise_intensity;
		float noise_scale;


		// auto factor = std::lerp(easeOutCirc(f), easeInQuint(1.0-f), f);
		// if (f < 0.5) {
			// current_radius = max_radius_ * easeInOutCirc(f);
			// current_radius = easeInOutCirc(f);
			// noise_intensity = easeInOutCirc(f);
		// } else if (f > 0.5) {
			// current_radius = max_radius_ * easeInOutCirc(1.0-f);
			// current_radius = max_radius_ * easeInOutCirc(1.0f-f);
		// }
// 		else {
// current_radius = max_radius_;
// 		}
		// noise_intensity = max_radius_ * f;
		// noise_scale = 0.5 * f;
		// current_radius = max_radius_ * factor;
		// auto factor = f <= 0.5f ? easeOutCirc(f/0.5f) : easeInQuint(1.0-(f/0.5));

		// auto factor = sin(f*M_PI);
		// current_radius = max_radius_ * factor;//sin(f*M_PI);
		// noise_intensity = 0.5 * std::clamp(factor, 0.0, 1.00);
		// noise_scale = 0.5*std::clamp(factor, 0.0, 1.00);

		// noise_intensity = std::clamp(easeInOutCirc(f), 0.5, 1.25);
		// noise_scale = std::clamp(easeInOutCirc(f), 0.25, 0.75);


		current_radius = max_radius_ * adsr(0.15f, 0.60f, f);
		noise_intensity = 0.5f * adsr(0.05f, 0.85f, f);
		noise_scale = 0.5f * adsr(0.05f, 0.60f, f);



		// if (f < 0.2f) {
		// 	current_radius = max_radius_ * (f / 0.2f);
		// 	noise_intensity = 1.0f * (f / 0.2f);
		// } else {
		// 	current_radius = max_radius_ * (1.0f + 0.2f * (f - 0.2f) / 0.8f);
		// 	// noise_intensity = 1.0f * (f / 0.2f);
		// 	noise_intensity = 1.0f * (1.0f - (f - 0.2f) / 0.8f);
		// }

		SdfSource source;
		source.position = glm::vec3(GetX(), GetY(), GetZ());
		source.radius = current_radius;
		source.color = glm::vec3(1.0f, 0.9f, 0.8f);
		source.smoothness = 1.0f;
		source.charge = 1.0f;
		source.type = 2; // TYPE_VOLUMETRIC
		source.noise_intensity = noise_intensity;
		source.noise_scale = noise_scale;

		manager_->UpdateSource(source_id_, source);
	}

	glm::mat4 SdfVolumetricExplosion::GetModelMatrix() const {
		return glm::translate(glm::mat4(1.0f), glm::vec3(GetX(), GetY(), GetZ()));
	}

	bool SdfVolumetricExplosion::IsExpired() const {
		return time_lived_ >= duration_;
	}

} // namespace Boidsish
