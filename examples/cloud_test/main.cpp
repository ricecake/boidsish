#include "graphics.h"
#include <vector>
#include <memory>

int main() {
    Boidsish::Visualizer visualizer(1024, 768, "Cloud Test");

    Boidsish::Camera camera;
    camera.x = 0.0f;
    camera.y = 50.0f;
    camera.z = 0.0f;
    camera.pitch = 30.0f; // Look up at clouds
    camera.yaw = 0.0f;
    visualizer.SetCamera(camera);

    visualizer.Run();
    return 0;
}
