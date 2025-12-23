#include "graphics.h"
#include "arrow.h"
#include <memory>
#include <cmath>
#include <vector>

int main(int argc, char* argv[]) {
    Boidsish::Visualizer vis;

    auto shape_function = [](float time) {
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

        // Arrow 1: Animating in a circle
        float x1 = 5.0f * cos(time);
        float z1 = 5.0f * sin(time);
        float y1 = 2.0f;
        shapes.push_back(std::make_shared<Boidsish::Arrow>(0, x1, y1, z1, 0.5f, 0.2f, 0.08f, 1.0f, 0.0f, 0.0f));

        // Arrow 2: Pointing straight down
        shapes.push_back(std::make_shared<Boidsish::Arrow>(1, 0.0f, -5.0f, 0.0f, 0.8f, 0.4f, 0.1f, 0.0f, 1.0f, 0.0f));

        // Arrow 3: A static arrow
        shapes.push_back(std::make_shared<Boidsish::Arrow>(2, 3.0f, 3.0f, 3.0f, 0.3f, 0.15f, 0.05f, 0.0f, 0.0f, 1.0f));

        return shapes;
    };

    vis.AddShapeHandler(shape_function);
    vis.Run();

    return 0;
}
