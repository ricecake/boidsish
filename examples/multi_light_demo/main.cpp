#include <iostream>
#include <vector>
#include <memory>
#include "graphics.h"
#include "dot.h"
#include "light.h"

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Multi-Light Demo");

        Boidsish::Light light1;
        light1.position = glm::vec3(5, 5, 5);
        light1.color = glm::vec3(1, 0, 0);
        light1.intensity = 1.0f;
        vis.GetLightManager().AddLight(light1);

        Boidsish::Light light2;
        light2.position = glm::vec3(-5, 5, 5);
        light2.color = glm::vec3(0, 1, 0);
        light2.intensity = 1.0f;
        vis.GetLightManager().AddLight(light2);

        Boidsish::Light light3;
        light3.position = glm::vec3(0, 5, -5);
        light3.color = glm::vec3(0, 0, 1);
        light3.intensity = 1.0f;
        vis.GetLightManager().AddLight(light3);

        vis.AddShapeHandler([&](float) {
            std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
            auto sphere = std::make_shared<Boidsish::Dot>(0, 0, 0, 2);
            shapes.push_back(sphere);
            return shapes;
        });

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
