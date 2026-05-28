#pragma once

#include "mood_manager.h"
#include "weather_constants.h"
#include <array>

namespace Boidsish {

    constexpr MoodLayer GetBaseTimeOfDayLayer() {
        MoodLayer layer;
        layer.name = "Base Time of Day";
        layer.priority = 0;
        layer.blendMode = MoodBlendMode::Multiply;
        layer.trackedParameter = MoodParameter::TimeOfDay;

        MoodSettings dawn;
        dawn.sceneBloom.targetLuminance = 1.0f;

        MoodSettings noon;
        noon.sceneBloom.targetLuminance = 0.95f;

        MoodSettings dusk;
        dusk.sceneBloom.targetLuminance = 1.0f;

        MoodSettings night;
        night.sceneBloom.targetLuminance = 1.5f;
        night.sceneBloom.maxExposure = 1.5f;

        layer.controlPoints.push_back({6.0f, dawn});
        layer.controlPoints.push_back({12.0f, noon});
        layer.controlPoints.push_back({18.0f, dusk});
        layer.controlPoints.push_back({24.0f, night});

        return layer;
    }

    constexpr MoodLayer GetWeatherPrecipitationLayer() {
        MoodLayer layer;
        layer.name = "Precipitation";
        layer.priority = 10;
        layer.blendMode = MoodBlendMode::Multiply;
        layer.trackedParameter = MoodParameter::Precipitation;

        MoodSettings clear;
        clear.sceneBloom.cdlSaturation = 1.0f;

        MoodSettings rain;
        rain.sceneBloom.cdlSaturation = 0.5f;

        layer.controlPoints.push_back({0.0f, clear});
        layer.controlPoints.push_back({1.0f, rain});

        return layer;
    }

    constexpr auto GetAllMoodSettings() {
        std::array items = {
            GetBaseTimeOfDayLayer(),
            GetWeatherPrecipitationLayer()
        };

        return items;
    }

} // namespace Boidsish
