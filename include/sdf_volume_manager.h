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
	};

	// GPU-friendly structure for UBO
	struct SdfSourceGPU {
		glm::vec4 position_radius;    // xyz: pos, w: radius
		glm::vec4 color_smoothness;   // rgb: color, a: smoothness
		glm::vec4 charge_type_unused; // x: charge, y: type, zw: unused
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

	private:
		GLuint ubo_ = 0;
		bool   initialized_ = false;

		std::map<int, SdfSource> sources_;
		int                      next_id_ = 0;

		static constexpr size_t kMaxSources = Constants::Class::SdfVolumes::MaxSources();
	};

} // namespace Boidsish
