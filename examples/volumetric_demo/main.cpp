#include "graphics.h"
#include "light_manager.h"
#include "weather_manager.h"
#include "ConfigManager.h"
#include <glm/glm.hpp>
#include <iostream>

using namespace Boidsish;

int main(int argc, char** argv) {
    Visualizer visualizer(1280, 720, "Volumetric Lighting Demo - Laser in Fog");

    visualizer.AddPrepareCallback([](Visualizer& v) {
        auto& lm = v.GetLightManager();
        auto& wm = *v.GetWeatherManager();

        // 1. Configure "Laser" Spotlight
        glm::vec3 spotPos(0.0f, 5.0f, 15.0f);
        glm::vec3 spotDir(0.0f, -0.1f, -1.0f);
        float     intensity = 100.0f;      // Very high intensity
        glm::vec3 color(0.0f, 1.0f, 0.5f); // Neon cyan/green laser
        float     innerAngle = 0.2f;       // Extremely narrow
        float     outerAngle = 0.5f;

        Light laser = Light::CreateSpot(spotPos, spotDir, intensity, color, innerAngle, outerAngle, true);
        lm.AddLight(laser);

        // 2. Configure Dense Aerosol (Fog)
        wm.SetManualPreset(3);                             // Foggy preset
        wm.SetTarget(WeatherAttribute::HazeDensity, 8.0f); // Boost haze density
        wm.SetTarget(WeatherAttribute::HazeHeight, 100.0f);
        wm.SetTarget(WeatherAttribute::MieScale, 4.0f);

        // Midnight for maximum drama
        lm.GetDayNightCycle().time = 0.0f;
        lm.GetDayNightCycle().paused = true;

        // 3. Add some obstacles to catch the beam and shadows
        for (int i = 0; i < 5; ++i) {
            auto box = std::make_shared<Boidsish::Shape>(Boidsish::ShapeType::CUBE);
            box->SetPosition(0.0f, 2.0f, -10.0f - i * 10.0f);
            box->SetScale(4.0f, 4.0f, 4.0f);
            box->SetColor(0.2f, 0.2f, 0.2f);
            v.AddShape(box);
        }

        // Enable floor to see the beam hitting the ground
        ConfigManager::GetInstance().SetBool("render_floor", true);
        ConfigManager::GetInstance().SetBool("enable_shadows", true);

        v.GetCamera().x = 15.0f;
        v.GetCamera().y = 8.0f;
        v.GetCamera().z = 25.0f;
        v.LookAt(glm::vec3(0.0f, 2.0f, 0.0f));
    });

    visualizer.Run();

    return 0;
}
