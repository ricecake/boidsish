#pragma once

#include <map>
#include <memory>
#include <vector>

#include "constants.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	struct SdfSource {
		glm::vec3 position;
		float     radius;
		glm::vec3 color;
		float     smoothness;
		float     charge; // Positive for union, negative for subtraction
		int       type;   // 0 for sphere, can add more later
		float     noise_intensity = 0.0f;
		float     noise_scale = 1.0f;
		float     density_cutoff = 1.0f;
		float     step_size_multiplier = 0.5f;
	};

	// GPU-friendly structure for UBO
	struct SdfSourceGPU {
		glm::vec4 position_radius;    // xyz: pos, w: radius
		glm::vec4 color_smoothness;   // rgb: color, a: smoothness
		glm::vec4 charge_type_noise;  // x: charge, y: type, z: noise_intensity, w: noise_scale
		glm::vec4 extra_params;       // x: density_cutoff, y: step_size_multiplier, zw: padding
	};

	class SdfVolumeManager {
	public:
		SdfVolumeManager();
		~SdfVolumeManager();

		void Initialize();
		void UpdateUBO();
		void BindUBO(GLuint binding_point) const;

		int  AddSource(const SdfSource& source);
		void UpdateSource(int id, const SdfSource& source);
		void RemoveSource(int id);
		void Clear();

		size_t GetSourceCount() const { return sources_.size(); }

		void SetNumNeighbors(int count) { num_neighbors_ = count; }
		int  GetNumNeighbors() const { return num_neighbors_; }

	private:
		GLuint ubo_ = 0;
		bool   initialized_ = false;

		std::map<int, SdfSource> sources_;
		int                      next_id_ = 0;
		int                      num_neighbors_ = 1;

		static constexpr size_t kMaxSources = Constants::Class::SdfVolumes::MaxSources();
	};

} // namespace Boidsish
