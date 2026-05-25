#pragma once

#include "mood_manager.h"
#include "weather_constants.h"

namespace Boidsish {

    inline MoodSettings GetDefaultMoodSettings() {
        MoodSettings s;
        s.sceneBloom = {};
        s.skyBloom = { .targetLuminance = 0.5f };

        s.cloudDensity = WeatherConstants::CloudDensity.normal;
        s.cloudAltitude = WeatherConstants::CloudAltitude.normal;
        s.cloudThickness = WeatherConstants::CloudThickness.normal;
        s.cloudColor = WeatherConstants::DefaultCloudColor;
        s.cloudCoverage = WeatherConstants::CloudCoverage.normal;
        s.cloudSunLightScale = 1.0f;
        s.cloudMoonLightScale = 2.0f;
        s.cloudPowderScale = 0.125f;
        s.cloudBeerPowderMix = 0.6f;

        s.rayleighScale = WeatherConstants::RayleighScale.normal;
        s.mieScale = WeatherConstants::MieScale.normal;
        s.rayleighScattering = WeatherConstants::RayleighScattering;
        s.mieScattering = WeatherConstants::MieScattering;
        s.mieExtinction = WeatherConstants::MieExtinction;

        return s;
    }

    inline MoodLayer GetBaseTimeOfDayLayer() {
        MoodLayer layer;
        layer.name = "Base Time of Day";
        layer.priority = 0;
        layer.blendMode = MoodBlendMode::Override;
        layer.trackedParameter = MoodParameter::TimeOfDay;

        // Dawn
        MoodSettings dawn = GetDefaultMoodSettings();
        dawn.sceneBloom.whiteTemp = 4000.0f;
        dawn.sceneBloom.cdlSlope = glm::vec3(1.1f, 1.0f, 0.9f);
        dawn.cloudColor = glm::vec3(1.0f, 0.7f, 0.5f);
        layer.controlPoints.push_back({6.0f, dawn});

        // Noon
        MoodSettings noon = GetDefaultMoodSettings();
        noon.sceneBloom.whiteTemp = 6500.0f;
        noon.cloudColor = glm::vec3(1.0f, 1.0f, 1.0f);
        layer.controlPoints.push_back({12.0f, noon});

        // Dusk
        MoodSettings dusk = GetDefaultMoodSettings();
        dusk.sceneBloom.whiteTemp = 3000.0f;
        dusk.sceneBloom.cdlSlope = glm::vec3(1.2f, 0.8f, 0.6f);
        dusk.cloudColor = glm::vec3(1.0f, 0.4f, 0.3f);
        layer.controlPoints.push_back({18.0f, dusk});

        // Night
        MoodSettings night = GetDefaultMoodSettings();
        night.sceneBloom.whiteTemp = 10000.0f;
        night.sceneBloom.cdlSlope = glm::vec3(0.5f, 0.6f, 1.0f);
        night.sceneBloom.targetLuminance = 0.05f;
        night.cloudColor = glm::vec3(0.1f, 0.1f, 0.2f);
        layer.controlPoints.push_back({24.0f, night});
        layer.controlPoints.push_back({0.0f, night});

        return layer;
    }

    inline MoodLayer GetWeatherPrecipitationLayer() {
        MoodLayer layer;
        layer.name = "Precipitation";
        layer.priority = 10;
        layer.blendMode = MoodBlendMode::Multiply;
        layer.trackedParameter = MoodParameter::Precipitation;

        MoodSettings clear = GetDefaultMoodSettings();
        // Multiply by 1.0 does nothing
        MoodSettings rain = GetDefaultMoodSettings();
        rain.sceneBloom.whiteTemp = 0.8f; // Cooling down
        rain.sceneBloom.cdlSaturation = 0.5f;
        rain.cloudColor = glm::vec3(0.4f, 0.4f, 0.45f);

        layer.controlPoints.push_back({0.0f, clear});
        layer.controlPoints.push_back({1.0f, rain});

        return layer;
    }

} // namespace Boidsish
