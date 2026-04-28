#pragma once

#include <map>
#include <memory>
#include <vector>

#include "IManager.h"
#include "constants.h"
#include "persistent_buffer.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class ServiceLocator;

	struct SdfSource {
		glm::vec3 position;
		float     radius;
		glm::vec3 color;
		float     smoothness;
		float     charge; // Positive for union, negative for subtraction
		int       type;   // 0 for sphere, etc.

		// Volumetric properties
		bool      volumetric = false;
		float     density = 1.0f;
		float     absorption = 0.5f;
		float     noise_scale = 0.1f;
		float     noise_intensity = 0.5f;
		glm::vec3 color_inner = glm::vec3(1.0f, 0.5f, 0.0f);
		glm::vec3 color_outer = glm::vec3(1.0f, 0.1f, 0.0f);

		// Animation/explosion parameters (packed into unused .w/.a slots on GPU)
		float normalized_time = 0.0f; // 0–1 progress through lifetime
		float emission = 0.0f;        // self-illumination intensity
		float ground_y = -1e6f;       // terrain floor (prevents explosion below ground)
	};

	// GPU-friendly structure for SSBO
	struct SdfSourceGPU {
		glm::vec4 position_radius;    // xyz: pos, w: radius
		glm::vec4 color_smoothness;   // rgb: color, a: smoothness
		glm::vec4 charge_type_vol_time; // x: charge, y: type, z: volumetric (0 or 1), w: time
		glm::vec4 volumetric_params;  // x: density, y: absorption, z: noise_scale, w: noise_intensity
		glm::vec4 color_inner;          // rgb: inner color, a: unused
		glm::vec4 color_outer;          // rgb: outer color, a: unused
	};

	struct SdfSsboData {
		int          count;
		int          padding[3];
		SdfSourceGPU sources[Constants::Class::SdfVolumes::MaxSources()];
	};

	class SdfVolumeManager: public IManager {
	public:
		SdfVolumeManager(ServiceLocator& loc);
		~SdfVolumeManager();

		void Initialize() override;
		void UpdateSSBO();
		void BindSSBO(GLuint binding_point) const;

		PersistentBuffer<SdfSsboData>& GetSSBO() { return *ssbo_; }

		int  AddSource(const SdfSource& source);
		void UpdateSource(int id, const SdfSource& source);
		void RemoveSource(int id);
		void Clear();

		size_t GetSourceCount() const { return sources_.size(); }

	private:
		std::unique_ptr<PersistentBuffer<SdfSsboData>> ssbo_;
		bool                                           initialized_ = false;

		std::map<int, SdfSource> sources_;
		int                      next_id_ = 0;

		static constexpr size_t kMaxSources = Constants::Class::SdfVolumes::MaxSources();
	};

} // namespace Boidsish
