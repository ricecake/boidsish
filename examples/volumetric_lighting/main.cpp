#include "graphics.h"
#include "service_locator.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/VolumetricLightingEffect.h"
#include "light_manager.h"
#include "weather_manager.h"
#include "terrain_generator_interface.h"

using namespace Boidsish;

int main() {
    Visualizer visualizer(1280, 720, "Volumetric Lighting Demo");

    // Add Volumetric Lighting effect
    auto vol_effect = std::make_shared<PostProcessing::VolumetricLightingEffect>();
    visualizer.GetPostProcessingManager().AddEffect(vol_effect);

    // Configure scene for god-rays
    auto& light_mgr = visualizer.GetLightManager();
    auto& cycle = light_mgr.GetDayNightCycle();
    cycle.time = 8.0f; // Morning sun
    cycle.speed = 0.0f; // Paused

    auto weather = visualizer.GetWeatherManager();
    if (weather) {
        // High humidity for visible rays
        weather->SetTarget(WeatherAttribute::Humidity, 0.8f);
        weather->SetTarget(WeatherAttribute::HazeDensity, 2.0f);
    }

    visualizer.Run();

    return 0;
}
