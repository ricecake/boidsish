#pragma once

#include <map>
#include <memory>
#include <vector>

#include "IManager.h"
#include "constants.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	enum class SdfType : int {
		Sphere = 0,
		Box = 1,
		Capsule = 2
	};

	struct SdfSource {
		glm::vec3 position = glm::vec3(0.0f);
		float     radius = 1.0f;
		glm::vec3 color = glm::vec3(1.0f);
		float     smoothness = 1.0f;
		float     charge = 1.0f; // Positive for union, negative for subtraction
		SdfType   type = SdfType::Sphere;

		// Primitive params
		glm::vec3 size = glm::vec3(1.0f); // Box dimensions or capsule height/radius
		float     height = 1.0f;

		// Volumetric properties
		bool      volumetric = false;
		float     density = 1.0f;
		float     absorption = 0.5f;
		float     noise_scale = 0.1f;
		float     noise_intensity = 0.5f;
		glm::vec3 color_inner = glm::vec3(1.0f, 0.5f, 0.0f);
		glm::vec3 color_outer = glm::vec3(1.0f, 0.1f, 0.0f);

		SdfSource() = default;
	};

	// GPU-friendly structure for SSBO
	struct SdfSourceGPU {
		glm::vec4 position_radius;      // xyz: pos, w: radius
		glm::vec4 color_smoothness;     // rgb: color, a: smoothness
		glm::vec4 charge_type_vol_unused; // x: charge, y: type, z: volumetric (0 or 1), w: unused
		glm::vec4 volumetric_params;    // x: density, y: absorption, z: noise_scale, w: noise_intensity
		glm::vec4 color_inner;          // rgb: inner color, a: unused
		glm::vec4 color_outer;          // rgb: outer color, a: unused
		glm::vec4 prim_params;          // xyz: size, w: height
	};

	class SdfVolumeManager: public IManager {
	public:
		SdfVolumeManager();
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
