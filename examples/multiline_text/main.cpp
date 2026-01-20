#include "graphics.h"
#include "text.h"
#include <memory>
#include <vector>
#include <GLFW/glfw3.h>

int main() {
    Boidsish::Visualizer visualizer(1280, 720, "Multiline Text Example");

    auto left_text = std::make_shared<Boidsish::Text>(
        "This is a\nleft-justified\ntext block.",
        "assets/fonts/Roboto-Regular.ttf",
        24.0f,
        5.0f,
        Boidsish::Text::LEFT,
        0,
        -400, 150, 0
    );

    auto center_text = std::make_shared<Boidsish::Text>(
        "This is a\ncentered\ntext block.",
        "assets/fonts/Roboto-Regular.ttf",
        24.0f,
        5.0f,
        Boidsish::Text::CENTER,
        0,
        0, 0, 0
    );

    auto right_text = std::make_shared<Boidsish::Text>(
        "This is a\nright-justified\ntext block.",
        "assets/fonts/Roboto-Regular.ttf",
        24.0f,
        5.0f,
        Boidsish::Text::RIGHT,
        0,
        400, -150, 0
    );

    visualizer.AddShape(left_text);
    visualizer.AddShape(center_text);
    visualizer.AddShape(right_text);

    double start_time = glfwGetTime();
    bool justification_changed = false;

    visualizer.AddShapeHandler([&](float delta_time) {
        double current_time = glfwGetTime();
        if (current_time - start_time > 3.0f && !justification_changed) {
            center_text->SetJustification(Boidsish::Text::LEFT);
            justification_changed = true;
        }
        return std::vector<std::shared_ptr<Boidsish::Shape>>();
    });

    visualizer.Run();

    return 0;
}
