#pragma once

#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class SdfVolumeManager;

	class SdfExplosion: public Shape {
	public:
		SdfExplosion(
			SdfVolumeManager* manager,
			const glm::vec3&  position,
			float             max_radius = 10.0f,
			float             duration = 2.0f
		);
		virtual ~SdfExplosion();

		void Update(float delta_time) override;

		// Shape interface
		void        render() const override {}
		void        render(Shader& /*shader*/, const glm::mat4& /*model_matrix*/) const override {}
		glm::mat4   GetModelMatrix() const override;
		std::string GetInstanceKey() const override { return "SdfExplosion"; }
		bool        IsExpired() const override;
		float       GetBoundingRadius() const override { return max_radius_ * 2.0f; }

	private:
		SdfVolumeManager* manager_;
		int               source_ids_[3];
		float             max_radius_;
		float             duration_;
		float             time_lived_ = 0.0f;

		glm::vec3 orbit_speeds_;
		glm::vec3 distance_rates_;
		float     base_radius_ = 0.0f;
		float     noise_intensity_ = 0.0f;
	};

} // namespace Boidsish
