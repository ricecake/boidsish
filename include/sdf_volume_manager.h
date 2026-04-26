#pragma once

#include <map>
#include <memory>
#include <vector>

#include "IManager.h"
#include "constants.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class ServiceLocator;

	enum class SdfHighType {
		Solid = 0,
		Volumetric = 1,
		Environmental = 2
	};

	struct SdfSource {
		glm::vec3 position;
		float     radius;
		glm::vec3 color;
		float     smoothness;
		float     charge = 1.0f; // Positive for union, negative for subtraction

		SdfHighType high_type = SdfHighType::Solid;
		int         sub_type = 0;

		// Flags: bit 0: Refraction, bit 1: Reflection
		int         flags = 0;

		float     density = 1.0f;
		float     absorption = 0.5f;
		float     noise_scale = 0.1f;
		float     noise_intensity = 0.5f;
		glm::vec3 color_inner = glm::vec3(1.0f, 0.5f, 0.0f);
		glm::vec3 color_outer = glm::vec3(1.0f, 0.1f, 0.0f);

		float normalized_time = 0.0f; // 0–1 progress through lifetime
		float emission = 0.0f;        // self-illumination intensity
		float ground_y = -1e6f;       // terrain floor

		glm::vec4 extra_params = glm::vec4(0.0f);
	};

	// GPU-friendly structure for SSBO
	struct SdfSourceGPU {
		glm::vec4 position_radius;      // xyz: pos, w: radius
		glm::vec4 color_smoothness;     // rgb: color, a: smoothness
		glm::vec4 high_sub_flags_charge; // x: high_type, y: sub_type, z: flags, w: charge
		glm::vec4 volumetric_params;    // x: density, y: absorption, z: noise_scale, w: noise_intensity
		glm::vec4 color_inner_emission;  // rgb: inner color, a: emission
		glm::vec4 color_outer_ground;    // rgb: outer color, a: ground_y
		glm::vec4 extra_params;
		glm::vec4 time_unused;          // x: normalized_time, yzw: unused
	};

	class SdfVolumeManager: public IManager {
	public:
		SdfVolumeManager(ServiceLocator& loc);
		~SdfVolumeManager();

		void Initialize() override;
		void UpdateSSBO();
		void BindSSBO(GLuint binding_point) const;

		int  AddSource(const SdfSource& source);
		void UpdateSource(int id, const SdfSource& source);
		void RemoveSource(int id);
		void Clear();

		size_t GetSourceCount() const { return sources_.size(); }

	private:
		GLuint ssbo_ = 0;
		bool   initialized_ = false;

		std::map<int, SdfSource> sources_;
		int                      next_id_ = 0;

		static constexpr size_t kMaxSources = Constants::Class::SdfVolumes::MaxSources();
	};

} // namespace Boidsish
