#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "walking_tank.h"
#include "SceneManager.h"
#include "task_thread_pool.hpp"
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
    Visualizer vis(1280, 720, "Walking Tank Demo");

    vis.AddPrepareCallback([&](Visualizer& v) {
        // Need a thread pool for EntityHandler
        static task_thread_pool::task_thread_pool thread_pool;
        auto handler = std::make_shared<EntityHandler>(thread_pool, std::shared_ptr<Visualizer>(&v, [](Visualizer*){}));

        // Add a walking tank at the center
        int tank_id = handler->AddEntity<WalkingTank>(0.0f, 1.0f, 0.0f);
        auto tank = std::dynamic_pointer_cast<WalkingTank>(handler->GetEntity(tank_id));

        // Set some lights
        v.GetLightManager().AddLight(Light::CreateDirectional(45.0f, 45.0f, 0.8f, glm::vec3(1.0f)));

        v.AddInputCallback([&, tank](const InputState& input) {
            if (input.mouse_button_down[0]) {
                auto worldPos = v.ScreenToWorld(input.mouse_x, input.mouse_y);
                if (worldPos) {
                    tank->SetTarget(*worldPos);
                }
            }
        });

        v.AddShapeHandler([handler, &vis](float dt) {
            float time = (float)glfwGetTime();
            (*handler)(time);
            return std::vector<std::shared_ptr<Shape>>{};
        });
    });

    vis.Run();
    return 0;
}
