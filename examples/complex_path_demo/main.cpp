#include <memory>
#include <vector>

#include "complex_path.h"
#include "graphics.h"
#include "terrain_generator.h"

int main(int argc, char** argv) {
    Boidsish::Visualizer visualizer(1024, 768, "Complex Path Demo");

    // Get the terrain generator and camera from the visualizer
    const Boidsish::TerrainGenerator* terrain_generator = visualizer.GetTerrainGenerator();
    Boidsish::Camera* camera = &visualizer.GetCamera();

    // Create the ComplexPath entity
    auto complex_path = std::make_shared<Boidsish::ComplexPath>(0, terrain_generator, camera);
    complex_path->SetVisible(true);

    // Create a shape handler to update and render the path
    visualizer.AddShapeHandler([=](float time) {
        complex_path->Update();
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
        shapes.push_back(complex_path);
        return shapes;
    });

    // Set a custom camera position
    Boidsish::Camera custom_camera;
    custom_camera.x = 16.0f;
    custom_camera.y = 20.0f;
    custom_camera.z = 16.0f;
    custom_camera.pitch = -45.0f;
    custom_camera.yaw = -45.0f;
    visualizer.SetCamera(custom_camera);

    visualizer.Run();

    return 0;
}
