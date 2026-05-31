#pragma once

#include "mood_manager.h"
#include "weather_constants.h"
#include <array>

namespace Boidsish {

    constexpr MoodLayer GetBaseTimeOfDayLayer() {
        MoodLayer timeOfDay;
        timeOfDay.name = "Base Time of Day";
        timeOfDay.priority = 0;
        timeOfDay.blendMode = MoodBlendMode::Multiply;
        timeOfDay.trackedParameter = MoodParameter::TimeOfDay;

        MoodSettings dawn;
        dawn.sceneBloom.targetLuminance = 1.0f;

        MoodSettings noon;
        noon.sceneBloom.targetLuminance = 0.95f;

        MoodSettings dusk;
        dusk.sceneBloom.targetLuminance = 1.0f;

        MoodSettings night;
        night.sceneBloom.targetLuminance = 1.5f;
        night.sceneBloom.maxExposure = 1.5f;

        timeOfDay.controlPoints.push_back({6.0f, dawn});
        timeOfDay.controlPoints.push_back({12.0f, noon});
        timeOfDay.controlPoints.push_back({18.0f, dusk});
        timeOfDay.controlPoints.push_back({24.0f, night});

        return timeOfDay;
    }

    constexpr MoodLayer GetWeatherPrecipitationLayer() {
        MoodLayer precipitation;
        precipitation.name = "Precipitation";
        precipitation.priority = 30;
        precipitation.blendMode = MoodBlendMode::Multiply;
        precipitation.trackedParameter = MoodParameter::Precipitation;

        MoodSettings clear;
        clear.sceneBloom.cdlSaturation = 1.0f;

        MoodSettings rain;
        rain.sceneBloom.cdlSaturation = 0.75f;

        precipitation.controlPoints.push_back({0.0f, clear});
        precipitation.controlPoints.push_back({1.0f, rain});

        return precipitation;
    }

    constexpr MoodLayer GetWeatherHumidityLayer() {
        MoodLayer Humidity;
        Humidity.name = "Humidity";
        Humidity.priority = 20;
        Humidity.blendMode = MoodBlendMode::Multiply;
        Humidity.trackedParameter = MoodParameter::Humidity;

        MoodSettings clear;
        clear.sceneBloom.cdlSaturation = 1.0f;

        MoodSettings humid;
        humid.sceneBloom.cdlSaturation = 1.25f;

        Humidity.controlPoints.push_back({0.0f, clear});
        Humidity.controlPoints.push_back({1.0f, humid});

        return Humidity;
    }

    constexpr MoodLayer GetWeatherCloudCoverLayer() {
        MoodLayer cloudCover;
        cloudCover.name = "Cloud Coverage";
        cloudCover.priority = 10;
        cloudCover.blendMode = MoodBlendMode::Multiply;
        cloudCover.trackedParameter = MoodParameter::CloudCover;

        MoodSettings clear;
        clear.sceneBloom.cdlSaturation = 1.0f;
        clear.sceneBloom.targetLuminance = 1.0f;

        MoodSettings overcast;
        overcast.sceneBloom.cdlSaturation = 0.75f;
        overcast.sceneBloom.targetLuminance = 0.85f;

        MoodSettings cloudy;
        cloudy.sceneBloom.cdlSaturation = 0.5f;
        cloudy.sceneBloom.targetLuminance = 0.5f;

        cloudCover.controlPoints.push_back({0.0f, clear});
        cloudCover.controlPoints.push_back({0.80f, overcast});
        cloudCover.controlPoints.push_back({1.0f, cloudy});

        return cloudCover;
    }

    constexpr auto GetAllMoodSettings() {
        std::array items = {
            GetBaseTimeOfDayLayer(),
            GetWeatherPrecipitationLayer(),
            GetWeatherHumidityLayer(),
            GetWeatherCloudCoverLayer(),
        };

        return items;
    }

} // namespace Boidsish
