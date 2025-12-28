#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <map>

class Shader;

namespace Boidsish {

class Shape;

// Stores the state of a single frozen clone.
struct CloneState {
    glm::mat4 model_matrix;
    glm::vec3 color;
    float creation_time;
    std::shared_ptr<const Shape> shape_ptr;
};

// Manages the creation, lifecycle, and rendering of clones for the freeze-frame trail effect.
class CloneManager {
public:
    CloneManager();

    // Creates a clone of the given shape at its current state.
    void CaptureClone(std::shared_ptr<const Shape> shape, float current_time);

    // Updates the clone list, removing expired or distant clones.
    void Update(float current_time, const glm::vec3& camera_pos);

    // Renders all active clones.
    void Render(Shader& shader);

private:
    std::vector<CloneState> clones_;

    float clone_lifespan_ = 2.0f; // in seconds
    float capture_interval_ = 0.2f; // in seconds
    size_t max_clones_global_ = 20; // This is a global limit for all clones
    float prune_distance_squared_ = 100.0f * 100.0f;

    std::map<int, float> last_capture_time_;
};

} // namespace Boidsish
