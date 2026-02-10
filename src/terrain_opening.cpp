#include "terrain_opening.h"
#include <algorithm>
#include <cstring>

namespace Boidsish {

    TerrainOpeningManager::TerrainOpeningManager() {
        std::memset(&ubo_data_, 0, sizeof(ubo_data_));
    }

    TerrainOpeningManager::~TerrainOpeningManager() {
        if (ubo_handle_) {
            glDeleteBuffers(1, &ubo_handle_);
        }
    }

    void TerrainOpeningManager::Initialize() {
        glGenBuffers(1, &ubo_handle_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_handle_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(OpeningData), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    int TerrainOpeningManager::AddOpening(const TerrainOpening& opening) {
        if (openings_.size() >= 16) return -1;
        openings_.push_back(opening);
        dirty_ = true;
        return static_cast<int>(openings_.size() - 1);
    }

    void TerrainOpeningManager::RemoveOpening(int id) {
        if (id >= 0 && id < static_cast<int>(openings_.size())) {
            openings_.erase(openings_.begin() + id);
            dirty_ = true;
        }
    }

    void TerrainOpeningManager::Clear() {
        openings_.clear();
        dirty_ = true;
    }

    void TerrainOpeningManager::UpdateUBO() {
        if (!dirty_) return;

        ubo_data_.num_openings = std::min(static_cast<int>(openings_.size()), 16);
        for (int i = 0; i < ubo_data_.num_openings; ++i) {
            ubo_data_.openings[i] = glm::vec4(openings_[i].center, openings_[i].radius);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_handle_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(OpeningData), &ubo_data_);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        dirty_ = false;
    }

    void TerrainOpeningManager::BindUBO(GLuint binding_point) {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_handle_);
    }

}
