#include "complex_path.h"

#include "graphics.h"
#include "shader.h"

namespace Boidsish {

ComplexPath::ComplexPath(int id, const TerrainGenerator* terrain_generator, Camera* camera)
    : Path(id)
    , terrain_generator_(terrain_generator)
    , camera_(camera)
{
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
        waypoints_.emplace_back(Waypoint{{point.x, point.y + height_, point.z}, {0, 1, 0}, 50.0f, 1.0f, 0.0f, 0.0f, 1.0f});
    }
}

void ComplexPath::SetHeight(float height)
{
    height_ = height;
    Update();
}

void ComplexPath::render() const
{
    if (!IsVisible() || GetWaypoints().size() < 2)
        return;

    if (!buffers_initialized_) {
        SetupBuffers();
    }

    shader->use();
    shader->setMat4("model", GetModelMatrix());

    glBindVertexArray(path_vao_);
    glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
    glBindVertexArray(0);
}

std::map<std::string, UniformValue> ComplexPath::GetRenderUniforms() const
{
    std::map<std::string, UniformValue> uniforms;
    uniforms["useGlowEffect"] = true;
    // uniforms["viewPos"] = glm::vec3(camera_->x, camera_->y, camera_->z);
    return uniforms;
}

} // namespace Boidsish
