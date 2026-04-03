#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "graphics.h"
#include "shape.h"

using namespace Boidsish;

class SdfExplosionDemo {
public:
    SdfExplosionDemo(Visualizer& viz) : viz_(viz) {
        viz_.AddInputCallback([this](const InputState& state) {
            if (state.key_down[GLFW_KEY_E]) {
                TriggerExplosion();
            }
        });
    }

    void TriggerExplosion() {
        glm::vec3 pos(
            (float(rand()) / RAND_MAX - 0.5f) * 20.0f,
            (float(rand()) / RAND_MAX) * 10.0f + 2.0f,
            (float(rand()) / RAND_MAX - 0.5f) * 20.0f
        );
        viz_.TriggerSdfExplosion(pos, 1.5f);
        std::cout << "Triggered SDF Explosion at (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }

private:
    Visualizer& viz_;
};

int main() {
    try {
        Visualizer viz(1280, 720, "SDF Volumetric Explosion Demo");

        Camera camera(0.0f, 10.0f, 30.0f, -15.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        SdfExplosionDemo demo(viz);

        std::cout << "SDF Explosion Demo Started!" << std::endl;
        std::cout << "Press 'E' to trigger a volumetric SDF explosion." << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Space/Shift - Up/Down" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  E - Trigger Explosion" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;

        // Trigger an initial one
        viz.AddPrepareCallback([](Visualizer& v) {
            v.TriggerSdfExplosion(glm::vec3(0.0f, 5.0f, 0.0f), 2.0f);
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
