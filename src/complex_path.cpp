#include "complex_path.h"

#include "graphics.h"
#include "shader.h"

namespace Boidsish {

std::shared_ptr<Shader> ComplexPath::complex_path_shader_ = nullptr;

ComplexPath::ComplexPath(int id, const TerrainGenerator* terrain_generator, Camera* camera)
    : Path(id)
    , terrain_generator_(terrain_generator)
    , camera_(camera)
{
    if (complex_path_shader_ == nullptr) {
        complex_path_shader_ = std::make_shared<Shader>(
            "shaders/complex_path.vert", "shaders/complex_path.frag"
        );
    }
    Update();
}

ComplexPath::~ComplexPath()
{
}

void ComplexPath::Update()
{
    if (!terrain_generator_ || !camera_) {
        return;
    }

    // Get camera's XZ position
    glm::vec2 camera_pos(camera_->x, camera_->z);

    // Get the terrain path closest to the camera
    auto terrainPath = terrain_generator_->GetPath(camera_pos, path_length_, segment_distance_);

    // Clear existing waypoints and add new ones from the terrain path
    waypoints_.clear();
    buffers_initialized_ = false;
    for (const auto& point : terrainPath) {
        waypoints_.emplace_back(Waypoint{{point.x, point.y + height_, point.z}, {0, 1, 0}, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    }
}

void ComplexPath::SetHeight(float height)
{
    height_ = height;
    Update();
}

void ComplexPath::render() const
{
    if (GetWaypoints().size() < 2)
        return;

    if (!buffers_initialized_) {
        SetupBuffers();
    }

    complex_path_shader_->use();
    complex_path_shader_->setMat4("model", GetModelMatrix());
    complex_path_shader_->setVec3("viewPos", glm::vec3(camera_->x, camera_->y, camera_->z));

    glBindVertexArray(path_vao_);
    glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
    glBindVertexArray(0);
}

} // namespace Boidsish
