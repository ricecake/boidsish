#include "graphics.h"
#include "text.h"
#include <memory>

int main() {
    Boidsish::Visualizer visualizer(1280, 720, "Text Example");

    auto text_shape = std::make_shared<Boidsish::Text>(
        "Hello, World!",
        "external/imgui/misc/fonts/Roboto-Medium.ttf",
        64.0f,
        10.0f,
        0,
        0.0f, -50.0f, 0.0f
    );
    text_shape->SetScale(glm::vec3(0.5f));
    visualizer.AddShape(text_shape);

    visualizer.GetCamera().z = 200.0f;

    while (!visualizer.ShouldClose()) {
        visualizer.Update();
    }

    return 0;
}
