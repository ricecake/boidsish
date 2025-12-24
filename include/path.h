#pragma once

#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

class Path : public Shape, public std::enable_shared_from_this<Path> {
public:
    struct Waypoint {
        Vector3 position;
        Vector3 up;
        float size;
        float r, g, b, a;
    };

    Path(int id = 0, float x = 0.0f, float y = 0.0f, float z = 0.0f);
    ~Path();

    void SetupBuffers() const;
    void render() const override;

    Waypoint& AddWaypoint(const Vector3& pos, const Vector3& up = Vector3(0, 1, 0), float size = 1.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) {
        waypoints_.emplace_back(Waypoint{pos, up.Normalized(), size, r, g, b, a});
        buffers_initialized_ = false;
        return waypoints_.back();
    }

    // Returns the desired change in heading and orientation
    std::pair<Vector3, glm::quat> CalculateUpdate(const Vector3& current_position, const glm::quat& current_orientation, float delta_time) const;

    bool IsLooping() const { return is_looping_; }
    void SetLooping(bool looping) { is_looping_ = looping; buffers_initialized_ = false; }

    std::vector<Waypoint>& GetWaypoints() { return waypoints_; }
    const std::vector<Waypoint>& GetWaypoints() const { return waypoints_; }

private:
    std::vector<Waypoint> waypoints_;
    bool is_looping_ = false;

    mutable GLuint path_vao_ = 0;
    mutable GLuint path_vbo_ = 0;
    mutable int    edge_vertex_count_ = 0;
    mutable bool   buffers_initialized_ = false;
};

class PathHandler {
public:
    PathHandler() = default;

    std::shared_ptr<Path> AddPath() {
        auto path = std::make_shared<Path>(paths_.size());
        paths_.push_back(path);
        return path;
    }

    const std::vector<std::shared_ptr<Path>>& GetPaths() const { return paths_; }

    std::vector<std::shared_ptr<Shape>> GetShapes() {
        std::vector<std::shared_ptr<Shape>> shapes;
        for (const auto& path : paths_) {
            shapes.push_back(path);
        }
        return shapes;
    }

private:
    std::vector<std::shared_ptr<Path>> paths_;
};

} // namespace Boidsish
