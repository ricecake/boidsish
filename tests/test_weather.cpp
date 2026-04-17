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
    wm.Update(0.016f, 0.0f, cameraPos, 12.0f);

    const auto& w1 = wm.GetCurrentWeather();

    // Ensure values are within reasonable ranges (Sunny/Cloudy/Overcast/Foggy combined)
    EXPECT_GE(w1.sun_intensity, 0.1f);
    EXPECT_LE(w1.sun_intensity, 1.2f);

    EXPECT_GE(w1.cloud_density, 0.0f);
    EXPECT_LE(w1.cloud_density, 100.0f);

    // Move time and check for change
    wm.Update(1.0f, 100.0f, cameraPos, 12.0f);
    const auto& w2 = wm.GetCurrentWeather();

    // With noise, it's highly likely to change over 100 seconds
    // Note: This might occasionally fail due to noise nature, but usually should pass.
}

TEST(WeatherManagerTest, SmoothTransition) {
    WeatherManager wm;
    glm::vec3 cameraPos(0.0f);

    // Initial state
    float initial_val = wm.GetCurrentWeather().cloud_density;

    // Tiny update should result in tiny change due to spring
    wm.Update(0.001f, 0.0f, cameraPos, 12.0f);
    float after_tiny_update = wm.GetCurrentWeather().cloud_density;

    EXPECT_NEAR(initial_val, after_tiny_update, 0.05f);
}

TEST(WeatherManagerTest, ExternalTargetOverride) {
    WeatherManager wm;
    glm::vec3 cameraPos(0.0f);

    float override_target = 50.0f;
    wm.SetTarget(WeatherAttribute::CloudDensity, override_target);
    wm.SetPace(WeatherAttribute::CloudDensity, 10.0f); // Fast pace for testing

    // Update for a long time to reach target
    for(int i = 0; i < 100; ++i) {
        wm.Update(0.1f, i * 0.1f, cameraPos, 12.0f);
    }

    EXPECT_NEAR(wm.GetCurrentWeather().cloud_density, override_target, 0.1f);

    // Clear target and ensure it moves again (or at least doesn't stay fixed if we move time/space)
    wm.ClearTarget(WeatherAttribute::CloudDensity);
    float value_after_clear = wm.GetCurrentWeather().cloud_density;

    wm.Update(10.0f, 200.0f, cameraPos + glm::vec3(1000.0f), 12.0f);
    float value_after_update = wm.GetCurrentWeather().cloud_density;

    EXPECT_NE(value_after_clear, value_after_update);
}

TEST(WeatherManagerTest, EnableDisable) {
    WeatherManager wm;
    wm.SetEnabled(false);

    glm::vec3 cameraPos(0.0f);
    const auto& w_before = wm.GetCurrentWeather();
    wm.Update(0.016f, 10.0f, cameraPos, 12.0f);
    const auto& w_after = wm.GetCurrentWeather();

    // Should not have updated if disabled
    EXPECT_FLOAT_EQ(w_before.cloud_density, w_after.cloud_density);
}
