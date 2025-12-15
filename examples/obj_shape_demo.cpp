#include "graphics.h"
#include "obj_shape.h"
#include <vector>
#include <memory>

int main() {
    Boidsish::Visualizer vis;

    vis.AddShapeHandler([&](float time) {
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

        // Create a few ObjShape cubes
        shapes.push_back(std::make_shared<Boidsish::ObjShape>("cube.obj", 0, -2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f));
        shapes.push_back(std::make_shared<Boidsish::ObjShape>("cube.obj", 1, 2.0f, 0.0f, 0.0f, 1.5f, 0.0f, 1.0f, 0.0f));
        shapes.push_back(std::make_shared<Boidsish::ObjShape>("cube.obj", 2, 0.0f, 2.0f, 0.0f, 0.75f, 0.0f, 0.0f, 1.0f));

        return shapes;
    });

    vis.Render();

    return 0;
}
