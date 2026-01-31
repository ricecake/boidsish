#include "sdf_volume_manager.h"
#include "logger.h"

namespace Boidsish {

    SdfVolumeManager::SdfVolumeManager() {}

    SdfVolumeManager::~SdfVolumeManager() {
        if (ubo_ != 0) {
            glDeleteBuffers(1, &ubo_);
        }
    }

    void SdfVolumeManager::Initialize() {
        if (initialized_) return;

        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);

        // Allocate space for: count (padded to 16 bytes) + array of SdfSourceGPU
        size_t ubo_size = 16 + kMaxSources * sizeof(SdfSourceGPU);
        glBufferData(GL_UNIFORM_BUFFER, ubo_size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        initialized_ = true;
        logger::INFO("SdfVolumeManager initialized");
    }

    void SdfVolumeManager::UpdateUBO() {
        if (!initialized_) return;

        std::vector<SdfSourceGPU> gpu_data;
        gpu_data.reserve(sources_.size());

        for (const auto& [id, source] : sources_) {
            if (gpu_data.size() >= kMaxSources) break;

            SdfSourceGPU data;
            data.position_radius = glm::vec4(source.position, source.radius);
            data.color_smoothness = glm::vec4(source.color, source.smoothness);
            data.charge_type_unused = glm::vec4(source.charge, static_cast<float>(source.type), 0.0f, 0.0f);
            gpu_data.push_back(data);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);

        // Update count
        int count = static_cast<int>(gpu_data.size());
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int), &count);

        // Update array data
        if (!gpu_data.empty()) {
            glBufferSubData(GL_UNIFORM_BUFFER, 16, gpu_data.size() * sizeof(SdfSourceGPU), gpu_data.data());
        }

        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void SdfVolumeManager::BindUBO(GLuint binding_point) const {
        if (ubo_ != 0) {
            glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_);
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
