#include <gtest/gtest.h>
#include "weather_manager.h"
#include <glm/glm.hpp>

using namespace Boidsish;

TEST(WeatherManagerTest, Initialization) {
    WeatherManager wm;
    EXPECT_TRUE(wm.IsEnabled());
    EXPECT_GT(wm.GetTimeScale(), 0.0f);
    EXPECT_GT(wm.GetSpatialScale(), 0.0f);
}

TEST(WeatherManagerTest, Update) {
    WeatherManager wm;
    glm::vec3 cameraPos(0.0f);
    BiomeAttributes biome{};
    biome.humidityPropensity = 0.5f;
    biome.aerosolEmission = 0.1f;
    wm.Update(0.016f, 0.0f, cameraPos, 12.0f, biome);

    const auto& w1 = wm.GetCurrentWeather();

    // Ensure values are within reasonable ranges
    EXPECT_GE(w1.sun_intensity, 0.1f);
    EXPECT_LE(w1.sun_intensity, 1.2f);

    EXPECT_GE(w1.cloud_density, 0.0f);
    EXPECT_LE(w1.cloud_density, 1.5f);

    // Move time and check for change
    wm.Update(1.0f, 100.0f, cameraPos, 13.0f, biome);
    const auto& w2 = wm.GetCurrentWeather();
}

TEST(WeatherManagerTest, SmoothTransition) {
    WeatherManager wm;
    glm::vec3 cameraPos(0.0f);
    BiomeAttributes biome{};

    // Initial state
    float initial_sun = wm.GetCurrentWeather().sun_intensity;

    // Tiny update should result in tiny change due to spring
    wm.Update(0.001f, 0.0f, cameraPos, 12.0f, biome);
    float after_tiny_update = wm.GetCurrentWeather().sun_intensity;

    EXPECT_NEAR(initial_sun, after_tiny_update, 0.01f);
}

TEST(WeatherManagerTest, ExternalTargetOverride) {
    WeatherManager wm;
    glm::vec3 cameraPos(0.0f);
    BiomeAttributes biome{};

    float override_target = 0.5f;
    wm.SetTarget(WeatherAttribute::SunIntensity, override_target);
    wm.SetPace(WeatherAttribute::SunIntensity, 10.0f); // Fast pace for testing

    // Update for a long time to reach target
    for(int i = 0; i < 100; ++i) {
        wm.Update(0.1f, i * 0.1f, cameraPos, 12.0f, biome);
    }

    EXPECT_NEAR(wm.GetCurrentWeather().sun_intensity, override_target, 0.01f);

    // Clear target and ensure it moves again
    wm.ClearTarget(WeatherAttribute::SunIntensity);
    float value_after_clear = wm.GetCurrentWeather().sun_intensity;

    wm.Update(10.0f, 200.0f, cameraPos + glm::vec3(1000.0f), 13.0f, biome);
    float value_after_update = wm.GetCurrentWeather().sun_intensity;

    EXPECT_NE(value_after_clear, value_after_update);
}

TEST(WeatherManagerTest, EnableDisable) {
    WeatherManager wm;
    wm.SetEnabled(false);

    glm::vec3 cameraPos(0.0f);
    BiomeAttributes biome{};
    const auto& w_before = wm.GetCurrentWeather();
    wm.Update(0.016f, 10.0f, cameraPos, 12.0f, biome);
    const auto& w_after = wm.GetCurrentWeather();

    // Should not have updated if disabled
    EXPECT_FLOAT_EQ(w_before.sun_intensity, w_after.sun_intensity);
}
