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
    wm.Update(0.016f, 0.0f, cameraPos);

    const auto w1 = wm.GetCurrentWeather();

    // Ensure values are within reasonable ranges (Sunny/Cloudy/Overcast/Foggy combined)
    EXPECT_GE(w1.sun_intensity, 0.1f);
    EXPECT_LE(w1.sun_intensity, 1.2f);

    EXPECT_GE(w1.cloud_density, 0.0f);
    EXPECT_LE(w1.cloud_density, 1.0f);

    // Move time and check for change
    wm.Update(1.0f, 100.0f, cameraPos);
    const auto w2 = wm.GetCurrentWeather();

    // With noise, it's highly likely to change over 100 seconds
    EXPECT_NE(w1.sun_intensity, w2.sun_intensity);
    EXPECT_NE(w1.wind_strength, w2.wind_strength);
}

TEST(WeatherManagerTest, EnableDisable) {
    WeatherManager wm;
    wm.SetEnabled(false);

    glm::vec3 cameraPos(0.0f);
    const auto& w_before = wm.GetCurrentWeather();
    wm.Update(0.016f, 10.0f, cameraPos);
    const auto& w_after = wm.GetCurrentWeather();

    // Should not have updated if disabled (current_ is zero-initialized or unchanged)
    EXPECT_FLOAT_EQ(w_before.sun_intensity, w_after.sun_intensity);
}
