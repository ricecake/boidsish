#include "complex_path.h"

#include "graphics.h"
#include "shader.h"
#include "entity.h"

namespace Boidsish {

ComplexPath::ComplexPath(int id, const TerrainGenerator* terrain_generator, Camera* camera)
    : Path(id)
    , terrain_generator_(terrain_generator)
    , camera_(camera)
    , height_(2.0f)
    , path_length_(300.0f)
    , segment_distance_(4.0f)
{
    Update();
}

ComplexPath::~ComplexPath()
{
}

void ComplexPath::Update()
{
    if (!terrain_generator_) {
        return;
    }

    // Get focus position (target entity or camera)
    glm::vec2 focus_pos;
    if (target_) {
        focus_pos = glm::vec2(target_->GetXPos(), target_->GetZPos());
    } else if (camera_) {
        focus_pos = glm::vec2(camera_->x, camera_->z);
    } else {
        return;
    }

    // Get the terrain path closest to the focus position
    // We request more points and smaller steps for a smoother terrain-hugging line
    int num_points = static_cast<int>(path_length_ / segment_distance_);
    auto terrainPath = terrain_generator_->GetPath(focus_pos, num_points, segment_distance_, max_curvature_, roughness_avoidance_);

    // Clear existing waypoints and add new ones from the terrain path
    waypoints_.clear();
    buffers_initialized_ = false;

    // We use a small vertical offset to prevent Z-fighting with the terrain
    for (const auto& point : terrainPath) {
        // Waypoint parameters: position, up, size, r, g, b, a
        // We use a larger size for the guide line to make it visible
        waypoints_.emplace_back(Waypoint{
            {point.x, point.y + height_, point.z},
            {0.0f, 1.0f, 0.0f},
            15.0f,
            0.2f, 0.8f, 1.0f, 0.6f // Light blue, semi-transparent
        });
    }
}

void ComplexPath::render() const
{
    if (shader) {
        render(*shader, GetModelMatrix());
    }
}

void ComplexPath::render(Shader& s, const glm::mat4& model_matrix) const
{
    if (!IsVisible() || GetWaypoints().size() < 2)
        return;

    if (!buffers_initialized_) {
        SetupBuffers();
    }

    // Set a very large cull radius to prevent the path from disappearing due to frustum culling
    s.setFloat("frustumCullRadius", 10000.0f);
    s.setMat4("model", model_matrix);
    s.setInt("useVertexColor", 1);

    glBindVertexArray(path_vao_);
    glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
    glBindVertexArray(0);

    s.setInt("useVertexColor", 0);
}

} // namespace Boidsish
