#include "graphics.h"
#include "shape.h"
#include "model.h"
#include "dot.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <vector>
#include <glm/gtc/random.hpp>

using namespace Boidsish;

int main() {
    std::cout << "Starting Complex Explosion Test..." << std::endl;
    Visualizer visualizer(1024, 768, "Complex Explosion Test");

    // Create a teapot model
    auto teapot = std::make_shared<Model>("assets/utah_teapot.obj");
    teapot->SetColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange teapot
    teapot->SetScale(glm::vec3(5.0f));
    teapot->SetPosition(0.0f, 10.0f, 0.0f);
    visualizer.AddShape(teapot);

    // Create a dot (procedural sphere)
    auto dot = std::make_shared<Dot>();
    dot->SetColor(0.0f, 0.8f, 1.0f, 1.0f); // Cyan dot
    dot->SetSize(10.0f);
    dot->SetPosition(30.0f, 10.0f, 0.0f);
    visualizer.AddShape(dot);

    auto glitter_dot = std::make_shared<Dot>();
    glitter_dot->SetColor(1.0f, 0.0f, 1.0f, 1.0f); // Magenta dot
    glitter_dot->SetSize(8.0f);
    glitter_dot->SetPosition(-30.0f, 10.0f, 0.0f);
    visualizer.AddShape(glitter_dot);

    bool exploded_teapot = false;
    bool exploded_dot = false;
    bool exploded_glitter = false;

    visualizer.AddInputCallback([&](const InputState& state) {
        if (state.key_down[GLFW_KEY_1] && !exploded_teapot) {
            std::cout << "Exploding Teapot with Standard Explosion!" << std::endl;
            visualizer.TriggerComplexExplosion(teapot, glm::vec3(0.0f, 1.0f, 0.0f), 2.0f, FireEffectStyle::Explosion);
            exploded_teapot = true;
        }
        if (state.key_down[GLFW_KEY_2] && !exploded_dot) {
            std::cout << "Exploding Cyan Dot with Sparks!" << std::endl;
            visualizer.TriggerComplexExplosion(dot, glm::vec3(1.0f, 0.5f, 0.0f), 1.5f, FireEffectStyle::Sparks);
            exploded_dot = true;
        }
        if (state.key_down[GLFW_KEY_3] && !exploded_glitter) {
            std::cout << "Exploding Magenta Dot with GLITTER!" << std::endl;
            visualizer.TriggerComplexExplosion(glitter_dot, glm::vec3(-1.0f, 1.0f, 0.0f), 3.0f, FireEffectStyle::Glitter);
            exploded_glitter = true;
        }
        if (state.key_down[GLFW_KEY_R]) {
            std::cout << "Resetting shapes..." << std::endl;
            if (exploded_teapot) {
                teapot = std::make_shared<Model>("assets/utah_teapot.obj");
                teapot->SetColor(1.0f, 0.5f, 0.0f, 1.0f);
                teapot->SetScale(glm::vec3(5.0f));
                teapot->SetPosition(0.0f, 10.0f, 0.0f);
                visualizer.AddShape(teapot);
                exploded_teapot = false;
            }
            if (exploded_dot) {
                dot = std::make_shared<Dot>();
                dot->SetColor(0.0f, 0.8f, 1.0f, 1.0f);
                dot->SetSize(10.0f);
                dot->SetPosition(30.0f, 10.0f, 0.0f);
                visualizer.AddShape(dot);
                exploded_dot = false;
            }
            if (exploded_glitter) {
                glitter_dot = std::make_shared<Dot>();
                glitter_dot->SetColor(1.0f, 0.0f, 1.0f, 1.0f);
                glitter_dot->SetSize(8.0f);
                glitter_dot->SetPosition(-30.0f, 10.0f, 0.0f);
                visualizer.AddShape(glitter_dot);
                exploded_glitter = false;
            }
        }
    });

    // Set a good camera position
    Camera cam;
    cam.x = 0.0f;
    cam.y = 40.0f;
    cam.z = 100.0f;
    cam.pitch = -20.0f;
    cam.yaw = 0.0f;
    visualizer.SetCamera(cam);

    std::cout << "Controls:" << std::endl;
    std::cout << "  1: Explode Teapot (Standard)" << std::endl;
    std::cout << "  2: Explode Cyan Dot (Sparks)" << std::endl;
    std::cout << "  3: Explode Magenta Dot (Glitter)" << std::endl;
    std::cout << "  R: Reset shapes" << std::endl;

    visualizer.Run();

    return 0;
}
