#include "graphics.h"
#include <cmath>
#include <tuple>

using namespace Boidsish;

int main() {
    Visualizer viz(1280, 720, "ReSTIR DI/GI Demo");

    viz.SetCameraMode(CameraMode::FREE);
    viz.GetCamera().y = 10.0f;
    viz.GetCamera().z = 30.0f;

    viz.AddPrepareCallback([](Visualizer& v) {
        // Add many flickering lights
        for (int i = 0; i < 100; i++) {
            float x = (rand() % 200 - 100);
            float z = (rand() % 200 - 100);
            auto [h, norm] = v.GetTerrainPropertiesAtPoint(x, z);

            Light l = Light::Create({x, h + 2.0f, z}, 5.0f, {1.0f, 0.5f, 0.2f}, false);
            l.SetFlicker(1.5f);
            v.GetLightManager().AddLight(l);
        }
    });

    viz.AddUpdateHandler([&](float time, float dt) {
        if (fmod(time, 2.0f) < dt) {
            // Trigger an explosion
            float x = (rand() % 100 - 50);
            float z = (rand() % 100 - 50);
            auto [h, norm] = viz.GetTerrainPropertiesAtPoint(x, z);
            viz.CreateExplosion({x, h, z}, 1.5f);
        }
    });

    viz.Run();
    return 0;
}
