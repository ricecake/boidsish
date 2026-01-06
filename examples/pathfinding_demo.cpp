#include "graphics.h"
#include "terrain_generator.h"
#include "pathfinder.h"
#include "path.h"
#include "dot.h"
#include <iostream>
#include <memory>
#include <vector>

class PathfindingDemo {
public:
    PathfindingDemo() : _gfx_started(false) {
        _gfx.AddInputCallback([this](const Boidsish::InputState& input) { HandleInput(input); });
        _terrain = std::make_unique<Boidsish::TerrainGenerator>();
    }

    void Run() {
        _gfx.AddShapeHandler([this](float time) { return Update(time); });
        _gfx_started = true;
        _gfx.Run();
    }

private:
    void HandleInput(const Boidsish::InputState& input) {
        // Input handling logic can be added here
    }

    std::vector<std::shared_ptr<Boidsish::Shape>> Update(float time) {
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

        if (!_path_calculated) {
            Boidsish::Pathfinder pathfinder(*_terrain);

            glm::vec3 start(10.0f, 0.0f, 10.0f);
            float startY = std::get<0>(_terrain->pointProperties(start.x, start.z));
            start.y = startY;

            glm::vec3 end(100.0f, 0.0f, 100.0f);
            float endY = std::get<0>(_terrain->pointProperties(end.x, end.z));
            end.y = endY;

            std::cout << "Calculating path from (" << start.x << ", " << start.z << ") to (" << end.x << ", " << end.z << ")" << std::endl;

            auto path_points = pathfinder.findPath(start, end);

            if (!path_points.empty()) {
                std::cout << "Path found, smoothing..." << std::endl;
                pathfinder.smoothPath(path_points);
                std::cout << "Path smoothed." << std::endl;

                _path = std::make_shared<Boidsish::Path>();
                for (const auto& p : path_points) {
                    _path->AddWaypoint({p.x, p.y, p.z});
                }
                _path->SetVisible(true);
            } else {
                std::cout << "No path found." << std::endl;
            }

            _start_dot = std::make_shared<Boidsish::Dot>(0, start.x, start.y, start.z, 2.0f, 1.0f, 0.0f, 0.0f);
            _end_dot = std::make_shared<Boidsish::Dot>(1, end.x, end.y, end.z, 2.0f, 0.0f, 1.0f, 0.0f);

            _path_calculated = true;
        }

        if (_path) {
            shapes.push_back(_path);
        }
        if (_start_dot) {
            shapes.push_back(_start_dot);
        }
        if (_end_dot) {
            shapes.push_back(_end_dot);
        }

        for (const auto& chunk : _terrain->getVisibleChunks()) {
            shapes.push_back(chunk);
        }

        return shapes;
    }

    Boidsish::Visualizer _gfx;
    std::unique_ptr<Boidsish::TerrainGenerator> _terrain;
    std::shared_ptr<Boidsish::Path> _path;
    std::shared_ptr<Boidsish::Dot> _start_dot;
    std::shared_ptr<Boidsish::Dot> _end_dot;
    bool _path_calculated = false;
    bool _gfx_started;
};

int main(int argc, char* argv[]) {
    PathfindingDemo demo;
    demo.Run();
    return 0;
}
