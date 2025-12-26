#include "graphics.h"
#include "model.h"
#include <vector>
#include <memory>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

int main(int argc, char** argv) {
    Boidsish::Visualizer vis;

    std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
    auto model = std::make_shared<Boidsish::Model>("assets/cube.obj");
    shapes.push_back(model);

    vis.AddShapeHandler([&](float time) {
        // --- Animate Position ---
        // Move the cube in a circle in the XZ plane.
        float radius = 2.0f;
        float posX = sin(time) * radius;
        float posZ = cos(time) * radius;
        model->SetPosition(posX, 0.0f, posZ);

        // --- Animate Rotation ---
        // Rotate the cube around the Y-axis.
        glm::quat rotation = glm::angleAxis(time, glm::vec3(0.0f, 1.0f, 0.0f));
        model->SetRotation(rotation);

        // --- Animate Scale ---
        // Make the cube pulse by scaling it up and down.
        float scale = 1.0f + 0.5f * sin(time * 2.0f);
        model->SetScale(glm::vec3(scale));

        return shapes;
    });

    vis.Run();

    return 0;
}
