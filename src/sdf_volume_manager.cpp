#include "sdf_volume_manager.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "frustum.h"
#include "logger.h"
#include "persistent_buffer.h"
#include "sdf_shape.h"

namespace Boidsish {

	SdfVolumeManager::SdfVolumeManager() {}

	SdfVolumeManager::~SdfVolumeManager() {}

	void SdfVolumeManager::Initialize() {
		if (initialized_)
			return;

		ssbo_ = std::make_unique<PersistentBuffer<SdfSourceGPU>>(GL_SHADER_STORAGE_BUFFER, kMaxSources);

		initialized_ = true;
		logger::INFO("SdfVolumeManager initialized with SSBO");
	}

	void SdfVolumeManager::UpdateFromShapes(const std::vector<std::shared_ptr<Shape>>& shapes, const Frustum& frustum) {
		if (!initialized_)
			return;

		ssbo_->AdvanceFrame();
		SdfSourceGPU* gpu_ptr = ssbo_->GetFrameDataPtr();

		std::vector<SdfSourceGPU> positives;
		std::vector<SdfSourceGPU> negatives;
		positives.reserve(512);
		negatives.reserve(128);

		glm::vec3 min_pt(std::numeric_limits<float>::max());
		glm::vec3 max_pt(std::numeric_limits<float>::lowest());
		bool      any_visible = false;

		for (const auto& shape : shapes) {
			if (!shape->IsSdf())
				continue;

			auto sdf_shape = std::static_pointer_cast<SdfShape>(shape);
			AABB aabb = sdf_shape->GetAABB();

			if (!frustum.IsBoxInFrustum(aabb.min, aabb.max))
				continue;

			SdfSourceGPU data;
			data.position_radius = glm::vec4(sdf_shape->GetX(), sdf_shape->GetY(), sdf_shape->GetZ(), sdf_shape->GetRadius());
			data.color_smoothness = glm::vec4(sdf_shape->GetR(), sdf_shape->GetG(), sdf_shape->GetB(), sdf_shape->GetSmoothness());
			data.charge_type_unused = glm::vec4(sdf_shape->GetCharge(), static_cast<float>(sdf_shape->GetSdfType()), 0.0f, 0.0f);

			if (sdf_shape->GetCharge() >= 0.0f) {
				if (positives.size() < kMaxSources) {
					positives.push_back(data);
				}
			} else {
				if (negatives.size() < kMaxSources) {
					negatives.push_back(data);
				}
			}

			min_pt = glm::min(min_pt, aabb.min);
			max_pt = glm::max(max_pt, aabb.max);
			any_visible = true;
		}

		num_positive_ = static_cast<int>(positives.size());
		num_negative_ = static_cast<int>(negatives.size());

		if (any_visible) {
			global_min_ = min_pt;
			global_max_ = max_pt;

			if (num_positive_ > 0) {
				std::memcpy(gpu_ptr, positives.data(), num_positive_ * sizeof(SdfSourceGPU));
			}
			if (num_negative_ > 0) {
				std::memcpy(gpu_ptr + num_positive_, negatives.data(), num_negative_ * sizeof(SdfSourceGPU));
			}
		} else {
			global_min_ = glm::vec3(0.0f);
			global_max_ = glm::vec3(0.0f);
		}
	}

	void SdfVolumeManager::BindSSBO(GLuint binding_point) const {
		if (ssbo_) {
			glBindBufferRange(
				GL_SHADER_STORAGE_BUFFER,
				binding_point,
				ssbo_->GetBufferId(),
				ssbo_->GetFrameOffset(),
				(num_positive_ + num_negative_) * sizeof(SdfSourceGPU)
			);
		}
	}

} // namespace Boidsish
