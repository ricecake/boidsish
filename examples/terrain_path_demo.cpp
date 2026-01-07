#include "graphics.h"
#include "terrain_generator.h"
#include <vector>
#include "dot.h"

int main(int argc, char** argv) {
    Boidsish::Visualizer visualizer(1024, 768, "Terrain Path Demo");

    // Get the terrain generator from the visualizer
    const Boidsish::TerrainGenerator* terrain_generator = visualizer.GetTerrainGenerator();

    if (terrain_generator) {
        // Get the path
        auto path = terrain_generator->GetPath({0, 0}, 200, 5.0f);

        // Create a shape handler to render the path
        visualizer.AddShapeHandler([path](float time) {
            std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
            for (const auto& point : path) {
                auto dot = std::make_shared<Boidsish::Dot>();
                dot->SetPosition(point.x, point.y + 0.2f, point.z);
                dot->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
                dot->SetSize(0.5f);
                shapes.push_back(dot);
            }
            return shapes;
        });
    }

    visualizer.Run();

    return 0;
}
