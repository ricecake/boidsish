#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "graphics.h"
#include "shape.h"

using namespace Boidsish;

class SdfPrimitiveDemo {
public:
    SdfPrimitiveDemo(Visualizer& viz) : viz_(viz) {
        viz_.AddInputCallback([this](const InputState& state) {
            if (state.key_down[GLFW_KEY_E]) {
                TriggerExplosion();
            }
        });
    }

    void SetupPrimitives() {
        // Sphere
        SdfSource sphere_src;
        sphere_src.position = glm::vec3(-10.0f, 5.0f, 0.0f);
        sphere_src.radius = 5.0f;
        sphere_src.color = glm::vec3(0.1f, 0.8f, 0.2f);
        sphere_src.type = SdfType::Sphere;
        viz_.AddShape(std::make_shared<SdfShape>(viz_.GetSdfVolumeManager(), sphere_src));

        // Box
        SdfSource box_src;
        box_src.position = glm::vec3(10.0f, 5.0f, 0.0f);
        box_src.size = glm::vec3(4.0f, 4.0f, 4.0f);
        box_src.color = glm::vec3(0.2f, 0.4f, 0.9f);
        box_src.type = SdfType::Box;
        viz_.AddShape(std::make_shared<SdfShape>(viz_.GetSdfVolumeManager(), box_src));

        // Capsule
        SdfSource capsule_src;
        capsule_src.position = glm::vec3(0.0f, 5.0f, -10.0f);
        capsule_src.radius = 2.0f;
        capsule_src.height = 8.0f;
        capsule_src.color = glm::vec3(0.9f, 0.2f, 0.2f);
        capsule_src.type = SdfType::Capsule;
        viz_.AddShape(std::make_shared<SdfShape>(viz_.GetSdfVolumeManager(), capsule_src));
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
        Visualizer viz(1280, 720, "SDF Volumetric & Primitive Demo");

        Camera camera(0.0f, 15.0f, 40.0f, -20.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        SdfPrimitiveDemo demo(viz);

        std::cout << "SDF Demo Started!" << std::endl;
        std::cout << "Press 'E' to trigger a volumetric SDF explosion." << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Space/Shift - Up/Down" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  E - Trigger Explosion" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;

        viz.AddPrepareCallback([&demo](Visualizer& v) {
            demo.SetupPrimitives();
            v.TriggerSdfExplosion(glm::vec3(0.0f, 5.0f, 0.0f), 2.0f);
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
