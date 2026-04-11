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
        // Position it slightly above ground, pointing horizontally or slightly down
        glm::vec3 spotPos(0.0f, 5.0f, 10.0f);
        glm::vec3 spotDir(0.0f, 0.0f, -1.0f);
        float intensity = 50.0f; // Very high intensity
        glm::vec3 color(0.0f, 1.0f, 0.5f); // Neon cyan/green laser
        float innerAngle = 0.5f; // Extremely narrow
        float outerAngle = 1.0f;

        Light laser = Light::CreateSpot(spotPos, spotDir, intensity, color, innerAngle, outerAngle, true);
        lm.AddLight(laser);

        // 2. Configure Dense Aerosol (Fog)
        wm.SetManualPreset(3); // Foggy preset
        wm.SetTarget(WeatherAttribute::HazeDensity, 5.0f); // Boost haze density
        wm.SetTarget(WeatherAttribute::HazeHeight, 50.0f);
        wm.SetTarget(WeatherAttribute::MieScale, 2.0f);

        // Disable Sun/Moon for dramatic effect if needed, or just let them be
        // For a laser demo, dark environment is better
        lm.GetDayNightCycle().time = 0.0f; // Midnight
        lm.GetDayNightCycle().paused = true;

        v.GetCamera().x = 10.0f;
        v.GetCamera().y = 5.0f;
        v.GetCamera().z = 20.0f;
        v.LookAt(spotPos + spotDir * 10.0f);
    });

    visualizer.Run();

    return 0;
}
