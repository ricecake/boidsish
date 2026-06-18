#include <gtest/gtest.h>
#include "weather_manager.h"
#include "service_locator.h"
#include "state_frame.h"

using namespace Boidsish;

TEST(WeatherManagerTest, Initialization) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    WeatherManager wm(loc);
    EXPECT_TRUE(wm.IsEnabled());
    EXPECT_FLOAT_EQ(wm.GetTimeScale(), 0.005f);
}

TEST(WeatherManagerTest, EnableDisable) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    WeatherManager wm(loc);
    wm.SetEnabled(false);

    glm::vec3 cameraPos(0.0f);
    const auto& w_before = wm.GetCurrentWeather();
    wm.Update(0.016f, 10.0f, cameraPos, 12.0f);
    const auto& w_after = wm.GetCurrentWeather();

    EXPECT_FLOAT_EQ(w_before.sun_intensity, w_after.sun_intensity);
}

TEST(WeatherManagerTest, ConstraintsLbm) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    WeatherManager wm(loc);
    wm.SetMacroSimEnabled(true);

    wm.SetMin(WeatherAttribute::Temperature, 310.0f);

    auto constraints = wm.GetSimConstraints();
    EXPECT_TRUE(constraints.temperature.min.has_value());
    EXPECT_GE(*constraints.temperature.min, 309.9f);

    wm.ClearAllConstraints();
    constraints = wm.GetSimConstraints();
    EXPECT_FALSE(constraints.temperature.min.has_value());
}

TEST(WeatherManagerTest, ContradictoryConstraints) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    WeatherManager wm(loc);

    wm.SetMin(WeatherAttribute::Temperature, 350.0f);
    wm.SetMax(WeatherAttribute::Temperature, 300.0f);

    auto constraints = wm.GetSimConstraints();
    EXPECT_TRUE(constraints.temperature.min.has_value() || constraints.temperature.max.has_value());
}
