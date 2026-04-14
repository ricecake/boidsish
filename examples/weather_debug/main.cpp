#include <iostream>
#include <iomanip>
#include <sstream>
#include "graphics.h"
#include "weather_manager.h"
#include "hud.h"

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Weather LBM Debug");

        auto weather = visualizer.GetWeatherManager();

        // Add HUD messages for weather info
        auto tempHud = visualizer.AddHudMessage("Temperature: --", HudAlignment::TOP_LEFT, {10, 10}, 1.5f);
        auto humHud = visualizer.AddHudMessage("Humidity: --", HudAlignment::TOP_LEFT, {10, 40}, 1.5f);
        auto windHud = visualizer.AddHudMessage("Wind: --", HudAlignment::TOP_LEFT, {10, 70}, 1.5f);
        auto aerosolHud = visualizer.AddHudMessage("Aerosol: --", HudAlignment::TOP_LEFT, {10, 100}, 1.5f);
        auto cloudHud = visualizer.AddHudMessage("Clouds: --", HudAlignment::TOP_LEFT, {10, 130}, 1.5f);
        auto timeHud = visualizer.AddHudMessage("Time: --", HudAlignment::TOP_LEFT, {10, 160}, 1.5f);

        visualizer.AddInputCallback([&](const InputState& state) {
            if (weather) {
                glm::vec3 camPos = visualizer.GetCamera().pos();
                auto localWeather = weather->GetWeatherAtPosition(camPos);

                std::stringstream ss;
                ss << std::fixed << std::setprecision(2);

                ss.str(""); ss << "Temperature: " << (localWeather.temperature - 273.15f) << " C (" << localWeather.temperature << " K)";
                tempHud->SetMessage(ss.str());

                ss.str(""); ss << "Humidity: " << (localWeather.humidity * 100.0f) << " %";
                humHud->SetMessage(ss.str());

                ss.str(""); ss << "Wind: [" << localWeather.windVelocity.x << ", " << localWeather.windVelocity.y << "] vWind: " << localWeather.verticalWind;
                windHud->SetMessage(ss.str());

                ss.str(""); ss << "Mie Scattering: " << localWeather.mieScattering;
                aerosolHud->SetMessage(ss.str());

                ss.str(""); ss << "Cloud Coverage: " << (localWeather.cloudCoverage * 100.0f) << " % Density: " << localWeather.cloudDensity;
                cloudHud->SetMessage(ss.str());

                ss.str(""); ss << "Time of Day: " << localWeather.timeOfDay << " h";
                timeHud->SetMessage(ss.str());
            }
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
