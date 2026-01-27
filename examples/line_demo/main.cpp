#include "graphics.h"
#include "line.h"
#include "vector.h"
#include <iostream>
#include <memory>
#include <cmath>

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Line Subclass Demo");

        // Setup camera
        Camera cam;
        cam.x = 0; cam.y = 5; cam.z = 20;
        cam.yaw = 0; cam.pitch = -10;
        visualizer.SetCamera(cam);

        // Add some lights
        // Light::Create(pos, intensity, color)
        visualizer.GetLightManager().AddLight(Light::Create(glm::vec3(5, 10, 5), 1.0f, glm::vec3(1, 1, 1)));
        visualizer.GetLightManager().SetAmbientLight(glm::vec3(0.1f, 0.1f, 0.1f));

        // 1. A simple solid line
        auto solidLine = std::make_shared<Line>(glm::vec3(-10, 0, 0), glm::vec3(-5, 5, 0), 0.2f);
        solidLine->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
        visualizer.AddShape(solidLine);

        // 2. A stylized laser (green) - Thicker to see glow
        auto laser1 = std::make_shared<Line>(glm::vec3(-2, 2, 0), glm::vec3(8, 2, 0), 0.5f);
        laser1->SetColor(0.0f, 1.0f, 0.0f, 1.0f);
        laser1->SetStyle(Line::Style::LASER);
        visualizer.AddShape(laser1);

        // 3. A stylized laser (blue) - Very thick
        auto laser2 = std::make_shared<Line>(glm::vec3(-5, 5, -5), glm::vec3(-5, -5, 5), 1.0f);
        laser2->SetColor(0.0f, 0.5f, 1.0f, 1.0f);
        laser2->SetStyle(Line::Style::LASER);
        visualizer.AddShape(laser2);

        // 4. A dynamic laser (yellow)
        auto laser3 = std::make_shared<Line>(glm::vec3(0, 0, 0), glm::vec3(0, 10, 0), 0.3f);
        laser3->SetColor(1.0f, 1.0f, 0.0f, 1.0f);
        laser3->SetStyle(Line::Style::LASER);
        visualizer.AddShape(laser3);

        int frame = 0;
        while (!visualizer.ShouldClose() && frame < 300) {
            float time = frame * 0.016f;

            // Animate the yellow laser
            laser3->SetEnd(glm::vec3(5.0f * std::sin(time), 10.0f, 5.0f * std::cos(time)));

            visualizer.Update();
            visualizer.Render();
            frame++;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
