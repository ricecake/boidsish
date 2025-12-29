#pragma once
#include "shader.h"
#include <glm/glm.hpp>
#include <memory>

namespace Boidsish {

class Visualizer;
class Line;
class Dot;

class DebugLaser {
public:
    DebugLaser(Visualizer& visualizer);
    ~DebugLaser();

    void Update(const struct InputState& input_state, const struct Camera& camera);
    void Render(Shader& shader);

    bool is_enabled_ = false;

private:
    bool RayTerrainIntersection(const glm::vec3& ray_origin, const glm::vec3& ray_dir, glm::vec3& out_intersection, glm::vec3& out_normal);

    Visualizer& visualizer_;

    std::shared_ptr<Line> initial_ray_;
    std::shared_ptr<Line> reflected_ray_;
    std::shared_ptr<Dot>  intersection_dot_;

    bool intersection_found_ = false;
};

} // namespace Boidsish
