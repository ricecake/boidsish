#include "clone_manager.h"
#include "shape.h"
#include "shader.h"
#include <algorithm>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace Boidsish {

CloneManager::CloneManager() {}

void CloneManager::CaptureClone(std::shared_ptr<const Shape> shape, float current_time) {
    if (shape->GetId() < 0) return;

    auto last_capture = last_capture_time_.find(shape->GetId());
    if (last_capture != last_capture_time_.end() && (current_time - last_capture->second) < capture_interval_) {
        return;
    }

    if (clones_.size() >= max_clones_global_) {
        return;
    }

    last_capture_time_[shape->GetId()] = current_time;

    CloneState new_clone;
    new_clone.model_matrix = shape->GetModelMatrix();
    new_clone.color = glm::vec3(shape->GetR(), shape->GetG(), shape->GetB());
    new_clone.creation_time = current_time;
    new_clone.shape_ptr = shape;

    clones_.push_back(new_clone);
}

void CloneManager::Update(float current_time, const glm::vec3& camera_pos) {
    clones_.erase(std::remove_if(clones_.begin(), clones_.end(),
        [&](const CloneState& clone) {
            if (current_time - clone.creation_time > clone_lifespan_) {
                return true;
            }
            glm::vec3 clone_pos = glm::vec3(clone.model_matrix[3]);
            if (glm::length2(camera_pos - clone_pos) > prune_distance_squared_) {
                 return true;
            }
            return false;
        }), clones_.end());

    if (clones_.size() > max_clones_global_) {
        std::sort(clones_.begin(), clones_.end(), [](const CloneState& a, const CloneState& b) {
            return a.creation_time > b.creation_time;
        });
        clones_.resize(max_clones_global_);
    }
}


void CloneManager::Render(Shader& shader) {
    shader.use();
    for (const auto& clone : clones_) {
        if (clone.shape_ptr) {
            clone.shape_ptr->render(shader, clone.model_matrix);
        }
    }
}

} // namespace Boidsish
