#include "graphics.h"
#include "arrow.h"
#include <memory>
#include <cmath>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

int main(int argc, char* argv[]) {
    Boidsish::Visualizer vis;

    auto shape_function = [](float time) {
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

        // Arrow 1: Animating in a circle with changing orientation
        auto arrow1 = std::make_shared<Boidsish::Arrow>(0, 0,0,0, 0.2f, 0.1f, 0.05f, 1.0f, 0.0f, 0.0f);
        float x1 = 5.0f * cos(time);
        float z1 = 5.0f * sin(time);
        float y1 = 2.0f;
        arrow1->SetPosition(x1, y1, z1);
        glm::quat orientation = glm::angleAxis(time, glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)));
        arrow1->SetRotation(orientation);
        arrow1->SetScale(glm::vec3(2.0f, 2.0f, 2.0f));
        shapes.push_back(arrow1);

        // Arrow 2: Static arrow pointing in a specific direction
        auto arrow2 = std::make_shared<Boidsish::Arrow>(1, 0,0,0, 0.2f, 0.1f, 0.05f, 0.0f, 1.0f, 0.0f);
        arrow2->SetPosition(0.0f, 2.0f, 0.0f);
        glm::quat orientation2 = glm::angleAxis(glm::pi<float>() / 4.0f, glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f)));
        arrow2->SetRotation(orientation2);
        arrow2->SetScale(glm::vec3(1.0f, 3.0f, 1.0f));
        shapes.push_back(arrow2);

        // Arrow 3: Another static arrow with different properties
        auto arrow3 = std::make_shared<Boidsish::Arrow>(2, 0,0,0, 0.2f, 0.1f, 0.05f, 0.0f, 0.0f, 1.0f);
        arrow3->SetPosition(-3.0f, 1.0f, -3.0f);
        glm::quat orientation3 = glm::angleAxis(glm::pi<float>() / 2.0f, glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f)));
        arrow3->SetRotation(orientation3);
        arrow3->SetScale(glm::vec3(1.5f, 1.5f, 1.5f));
        shapes.push_back(arrow3);

        return shapes;
    };

    vis.AddShapeHandler(shape_function);
    vis.Run();

    return 0;
}
