#pragma once

#include "mood_manager.h"
#include "weather_constants.h"

namespace Boidsish {

    inline MoodSettings GetDefaultMoodSettings() {
        MoodSettings s;
        // Start with everything unassigned
        return s;
    }

    inline MoodLayer GetBaseTimeOfDayLayer() {
        MoodLayer layer;
        layer.name = "Base Time of Day";
        layer.priority = 0;
        layer.blendMode = MoodBlendMode::Override;
        layer.trackedParameter = MoodParameter::TimeOfDay;

        // Dawn
        MoodSettings dawn;
        dawn.sceneBloom.whiteTemp = 4000.0f;
        dawn.sceneBloom.cdlSlope = glm::vec3(1.1f, 1.0f, 0.9f);
        dawn.cloudColor = glm::vec3(1.0f, 0.7f, 0.5f);
        dawn.cloudDensity = WeatherConstants::CloudDensity.normal;
        dawn.cloudCoverage = WeatherConstants::CloudCoverage.normal;
        dawn.rayleighScale = WeatherConstants::RayleighScale.normal;
        dawn.mieScale = WeatherConstants::MieScale.normal;
        layer.controlPoints.push_back({6.0f, dawn});

        // Noon
        MoodSettings noon;
        noon.sceneBloom.whiteTemp = 6500.0f;
        noon.cloudColor = glm::vec3(1.0f, 1.0f, 1.0f);
        noon.cloudDensity = WeatherConstants::CloudDensity.normal;
        noon.cloudCoverage = WeatherConstants::CloudCoverage.normal;
        noon.rayleighScale = WeatherConstants::RayleighScale.normal;
        noon.mieScale = WeatherConstants::MieScale.normal;
        layer.controlPoints.push_back({12.0f, noon});

        // Dusk
        MoodSettings dusk;
        dusk.sceneBloom.whiteTemp = 3000.0f;
        dusk.sceneBloom.cdlSlope = glm::vec3(1.2f, 0.8f, 0.6f);
        dusk.cloudColor = glm::vec3(1.0f, 0.4f, 0.3f);
        dusk.cloudDensity = WeatherConstants::CloudDensity.normal;
        dusk.cloudCoverage = WeatherConstants::CloudCoverage.normal;
        dusk.rayleighScale = WeatherConstants::RayleighScale.normal;
        dusk.mieScale = WeatherConstants::MieScale.normal;
        layer.controlPoints.push_back({18.0f, dusk});

        // Night
        MoodSettings night;
        night.sceneBloom.whiteTemp = 10000.0f;
        night.sceneBloom.cdlSlope = glm::vec3(0.5f, 0.6f, 1.0f);
        night.sceneBloom.targetLuminance = 0.05f;
        night.cloudColor = glm::vec3(0.1f, 0.1f, 0.2f);
        night.cloudDensity = WeatherConstants::CloudDensity.normal;
        night.cloudCoverage = WeatherConstants::CloudCoverage.normal;
        night.rayleighScale = WeatherConstants::RayleighScale.normal;
        night.mieScale = WeatherConstants::MieScale.normal;
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

        MoodSettings clear;
        // Multiply by 1.0 effectively does nothing if we set it,
        // but if we don't set it, it's not applied at all.
        // Actually for Multiply, if we want to "do nothing" we should set to 1.0.
        clear.sceneBloom.whiteTemp = 1.0f;
        clear.sceneBloom.cdlSaturation = 1.0f;
        clear.cloudColor = glm::vec3(1.0f);
        layer.controlPoints.push_back({0.0f, clear});

        MoodSettings rain;
        rain.sceneBloom.whiteTemp = 0.8f;
        rain.sceneBloom.cdlSaturation = 0.5f;
        rain.cloudColor = glm::vec3(0.4f, 0.4f, 0.45f);
        layer.controlPoints.push_back({1.0f, rain});

        return layer;
    }

} // namespace Boidsish
