#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "graphics.h"
#include "shape.h"
#include "dot.h"

using namespace Boidsish;

std::vector<std::shared_ptr<Shape>> BackgroundShapes(float time) {
    std::vector<std::shared_ptr<Shape>> shapes;
    for (int i = 0; i < 50; ++i) {
        float angle = (float)i * (2.0f * 3.14159f / 25.0f) + time * 0.2f;
        float x = cos(angle) * 15.0f;
        float z = sin(angle) * 15.0f;
        float y = 2.0f + sin(time + (float)i * 0.5f) * 5.0f;

        auto dot = std::make_shared<Dot>(100 + i, x, y, z, 50.0f);
        dot->SetColor(
            0.5f + 0.5f * cos(angle),
            0.5f + 0.5f * sin(angle),
            0.5f + 0.5f * sin(time),
            1.0f
        );
        shapes.push_back(dot);
    }
    return shapes;
}

int main() {
    try {
        Visualizer viz(1280, 720, "SDF Volumetric Refraction Demo");

        Camera camera(0.0f, 5.0f, 25.0f, -10.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        viz.AddShapeHandler(BackgroundShapes);

        std::cout << "SDF Refraction Demo Started!" << std::endl;

        viz.AddPrepareCallback([](Visualizer& v) {
            SdfSource source;
            source.position = glm::vec3(0.0f, 5.0f, 0.0f);
            source.radius = 10.0f;
            source.color = glm::vec3(0.1f, 0.4f, 0.8f);
            source.smoothness = 1.0f;
            source.charge = 1.0f;
            source.type = 0;

            source.volumetric = true;
            source.density = 0.5f;
            source.absorption = 0.1f;
            source.noise_scale = 0.2f;
            source.noise_intensity = 0.5f;
            source.color_inner = glm::vec3(0.2f, 0.6f, 1.0f);
            source.color_outer = glm::vec3(0.0f, 0.2f, 0.5f);
            source.emission = 0.0f;
            source.ground_y = -100.0f;
            source.normalized_time = 0.5f; // Static mushroom-ish shape
            source.refraction_strength = 0.15f; // Positive for outward bending (magnifying)

            v.AddSdfSource(source);
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
