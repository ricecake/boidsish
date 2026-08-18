#include <iostream>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "graphics.h"
#include "service_locator.h"
#include "volumetric_fire_smoke_manager.h"
#include "light_manager.h"
#include "dot.h"

using namespace Boidsish;

int main() {
    Visualizer viz(1280, 720, "Volumetric Fire & Smoke Demo");

    viz.AddPrepareCallback([](Visualizer& v) {
        auto fireSmokeMgr = ServiceLocator::Instance().Get<VolumetricFireSmokeManager>();
        if (fireSmokeMgr) {
            // Position fire volume over terrain
            fireSmokeMgr->SetWorldBounds(glm::vec3(0.0f, 15.0f, 0.0f), glm::vec3(120.0f, 60.0f, 120.0f));
            fireSmokeMgr->ResetEffectMap(1.0f); // 100% fuel coverage
            fireSmokeMgr->Ignite(glm::vec3(0.0f, 2.0f, 0.0f), 10.0f, 1.0f);
        }

        auto lightMgr = ServiceLocator::Instance().Get<LightManager>();
        if (lightMgr) {
            auto& cycle = lightMgr->GetDayNightCycle();
            cycle.enabled = true;
            cycle.time = 18.5f; // Sunset/dusk for dramatic volumetric light shafts and fire glow
        }

        v.GetCamera().x = 0.0f;
        v.GetCamera().y = 20.0f;
        v.GetCamera().z = 60.0f;
        v.LookAt(glm::vec3(0.0f, 10.0f, 0.0f));
    });

    // Space key triggers new ignitions
    viz.AddInputCallback([](const InputState& state) {
        if (state.key_down[GLFW_KEY_SPACE]) {
            auto fireSmokeMgr = ServiceLocator::Instance().Get<VolumetricFireSmokeManager>();
            if (fireSmokeMgr) {
                // Ignite a random spot on the terrain
                float rx = (float(rand() % 100) / 100.0f - 0.5f) * 60.0f;
                float rz = (float(rand() % 100) / 100.0f - 0.5f) * 60.0f;
                fireSmokeMgr->Ignite(glm::vec3(rx, 2.0f, rz), 8.0f, 1.0f);
                std::cout << "[Volumetric Fire Demo] Ignited fire at (" << rx << ", 2.0, " << rz << ")\n";
            }
        }
    });

    std::cout << "\n=======================================================\n";
    std::cout << " Volumetric Fire & Smoke Demo\n";
    std::cout << " Controls:\n";
    std::cout << "   SPACE: Ignite dynamic fire on effect map\n";
    std::cout << "   WASD: Move camera, Mouse: Look around\n";
    std::cout << "   `: Toggle ImGui Environment window for tuning\n";
    std::cout << "=======================================================\n\n";

    viz.Run();
    return 0;
}
