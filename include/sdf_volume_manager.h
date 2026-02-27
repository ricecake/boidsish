#pragma once

#include <memory>
#include <vector>

#include "constants.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	class Shape;
	class Frustum;
	template <typename T>
	class PersistentBuffer;

	struct SdfSource {
		glm::vec3 position;
		float     radius;
		glm::vec3 color;
		float     smoothness;
		float     charge; // Positive for union, negative for subtraction
		int       type;   // 0 for sphere, can add more later
	};

	// GPU-friendly structure for SSBO
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
		void UpdateFromShapes(const std::vector<std::shared_ptr<Shape>>& shapes, const Frustum& frustum);
		void BindSSBO(GLuint binding_point) const;

		void GetBoundingBox(glm::vec3& min, glm::vec3& max) const {
			min = global_min_;
			max = global_max_;
		}

		void GetSourceCounts(int& positive, int& negative) const {
			positive = num_positive_;
			negative = num_negative_;
		}

	private:
		std::unique_ptr<PersistentBuffer<SdfSourceGPU>> ssbo_;
		bool                                            initialized_ = false;

		glm::vec3 global_min_{0.0f};
		glm::vec3 global_max_{0.0f};
		int       num_positive_ = 0;
		int       num_negative_ = 0;

		static constexpr size_t kMaxSources = 4096;
	};

} // namespace Boidsish
