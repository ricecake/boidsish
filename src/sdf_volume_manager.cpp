#include "sdf_volume_manager.h"

#include <cstring>

#include "logger.h"
#include "service_locator.h"
#include "profiler.h"

namespace Boidsish {

	SdfVolumeManager::SdfVolumeManager(ServiceLocator& /*loc*/) {}

	SdfVolumeManager::~SdfVolumeManager() {}

	void SdfVolumeManager::Initialize() {
		if (initialized_)
			return;

		ssbo_ = std::make_unique<PersistentBuffer<SdfSsboData>>(GL_SHADER_STORAGE_BUFFER, 1, 3);
		memset(ssbo_->GetFullBufferPtr(), 0, ssbo_->GetTotalSize());

		initialized_ = true;
		logger::INFO("SdfVolumeManager initialized with SSBO");
	}

	void SdfVolumeManager::UpdateSSBO() {
		PROJECT_PROFILE_SCOPE("SdfVolumeManager::UpdateSSBO");
		if (!initialized_)
			return;

		ssbo_->AdvanceFrame();
		SdfSsboData* data_ptr = ssbo_->GetFrameDataPtr();
		int          count = 0;

		for (const auto& [id, source] : sources_) {
			if (count >= kMaxSources)
				break;

			SdfSourceGPU& data = data_ptr->sources[count];
			data.position_radius = glm::vec4(source.position, source.radius);
			data.color_smoothness = glm::vec4(source.color, source.smoothness);
			data.charge_type_vol_time = glm::vec4(
				source.charge,
				static_cast<float>(source.type),
				source.volumetric ? 1.0f : 0.0f,
				source.normalized_time
			);
			data.volumetric_params = glm::vec4(
				source.density,
				source.absorption,
				source.noise_scale,
				source.noise_intensity
			);
			data.color_inner = glm::vec4(source.color_inner, source.emission);
			data.color_outer = glm::vec4(source.color_outer, source.ground_y);
			count++;
		}

		data_ptr->count = count;
	}

	void SdfVolumeManager::BindSSBO(GLuint binding_point) const {
		if (ssbo_) {
			ssbo_->BindRange(binding_point);
		}
	}

	int SdfVolumeManager::AddSource(const SdfSource& source) {
		int id = next_id_++;
		sources_[id] = source;
		return id;
	}

	void SdfVolumeManager::UpdateSource(int id, const SdfSource& source) {
		if (sources_.find(id) != sources_.end()) {
			sources_[id] = source;
		}
	}

	void SdfVolumeManager::RemoveSource(int id) {
		sources_.erase(id);
	}

	void SdfVolumeManager::Clear() {
		sources_.clear();
	}

} // namespace Boidsish
