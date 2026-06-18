#include <gtest/gtest.h>
#include "weather_manager.h"
#include "lightning_manager.h"
#include "light_manager.h"
#include "service_locator.h"
#include "state_frame.h"
#include <iostream>

using namespace Boidsish;

TEST(WeatherRegressionTest, StrictEnforcementPrecipitationAndLightning) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    auto ltm = std::make_shared<LightManager>(loc);
    loc.Provide<LightManager>(ltm);

    auto lm = std::make_shared<LightningManager>(loc);
    loc.Provide<LightningManager>(lm);

    WeatherManager wm(loc);
    wm.SetMacroSimEnabled(false); // Disable LBM for predictable attribute testing

    // 1. Set high precipitation target
    wm.SetTarget(WeatherAttribute::Precipitation, 1.0f);
    wm.SetPace(WeatherAttribute::Precipitation, 1000.0f); // Fast pace

    glm::vec3 cameraPos(0.0f);
    // Update multiple times to ensure it reaches target if not snapped
    for(int i=0; i<10; ++i) wm.Update(0.016f, (float)i*0.016f, cameraPos, 12.0f);

    EXPECT_GT(wm.GetCurrentWeather().precipitation, 0.9f);

    // 2. Enable strict enforcement and set precipitation target to 0
    wm.SetStrictEnforcement(true);
    wm.SetTarget(WeatherAttribute::Precipitation, 0.0f);

    // Clear any strikes that might have spawned in step 1
    lm->Update(100.0f, 100.0f);
    EXPECT_EQ(lm->GetActiveStrikes().size(), 0);

    // 3. Update once. It should snap to 0 immediately and NOT trigger lightning.
    // We'll use a large deltaTime to increase strike chance if it were still > 0.4
    wm.Update(1.0f, 2.0f, cameraPos, 12.0f);

    EXPECT_FLOAT_EQ(wm.GetCurrentWeather().precipitation, 0.0f);
    EXPECT_EQ(lm->GetActiveStrikes().size(), 0);
}

TEST(WeatherRegressionTest, ConstraintClamping) {
    ServiceLocator loc;
    ServiceLocator::SetInstance(&loc);

    auto fb = std::make_shared<state::FrameBuffer>();
    loc.Provide<state::FrameBuffer>(fb);

    auto ltm = std::make_shared<LightManager>(loc);
    loc.Provide<LightManager>(ltm);

    auto lm = std::make_shared<LightningManager>(loc);
    loc.Provide<LightningManager>(lm);

    WeatherManager wm(loc);
    wm.SetMacroSimEnabled(false);

    // Set a high target but a low max constraint
    wm.SetTarget(WeatherAttribute::Precipitation, 1.0f);
    wm.SetMax(WeatherAttribute::Precipitation, 0.2f);
    wm.SetPace(WeatherAttribute::Precipitation, 1000.0f);

    glm::vec3 cameraPos(0.0f);
    wm.Update(0.016f, 0.016f, cameraPos, 12.0f);

    // Should be clamped to 0.2 even if target is 1.0
    EXPECT_LE(wm.GetCurrentWeather().precipitation, 0.2001f);
}
