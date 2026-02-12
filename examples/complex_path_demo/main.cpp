#include <memory>
#include <vector>

#include "complex_path.h"
#include "graphics.h"
#include "terrain_generator.h"

int main(int argc, char** argv) {
    Boidsish::Visualizer visualizer(1024, 768, "Complex Path Demo");

    // Get the terrain generator and camera from the visualizer
    auto terrain_ptr = visualizer.GetTerrain();
    const Boidsish::TerrainGenerator* terrain_generator = dynamic_cast<const Boidsish::TerrainGenerator*>(terrain_ptr.get());
    Boidsish::Camera* camera = &visualizer.GetCamera();

    // // Set a custom camera position
    // Boidsish::Camera custom_camera;
    // custom_camera.x = 16.0f;
    // custom_camera.y = 20.0f;
    // custom_camera.z = 16.0f;
    // custom_camera.pitch = -45.0f;
    // custom_camera.yaw = -45.0f;
    // visualizer.SetCamera(custom_camera);


    // Create the ComplexPath entity
    auto complex_path = std::make_shared<Boidsish::ComplexPath>(0, terrain_generator, camera);
    complex_path->SetVisible(true);

    // Set some advanced parameters
    complex_path->SetMaxCurvature(0.5f); // Smoother turns
    complex_path->SetRoughnessAvoidance(0.8f); // Avoid steep terrain
    complex_path->SetPathLength(500.0f); // Longer guide line

    // Create a shape handler to update and render the path
    visualizer.AddShapeHandler([=](float time) {
        // Adjust parameters based on time to show dynamic nature
        float avoidance = 0.5f + 0.5f * sin(time * 0.5f);
        complex_path->SetRoughnessAvoidance(avoidance);

        complex_path->Update();
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
        shapes.push_back(complex_path);
        return shapes;
    });


    visualizer.Run();

    return 0;
}
