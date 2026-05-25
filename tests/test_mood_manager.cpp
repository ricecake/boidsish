#include <gtest/gtest.h>
#include "mood_manager.h"
#include "mood_definitions.h"

using namespace Boidsish;

TEST(MoodManagerTest, BasicInterpolation) {
    MoodManager mgr;
    MoodLayer layer;
    layer.name = "Test Layer";
    layer.priority = 0;
    layer.blendMode = MoodBlendMode::Override;
    layer.trackedParameter = MoodParameter::TimeOfDay;

    MoodSettings s0 = GetDefaultMoodSettings();
    s0.cloudDensity = 0.0f;
    s0.cloudColor = glm::vec3(0.0f);

    MoodSettings s1 = GetDefaultMoodSettings();
    s1.cloudDensity = 1.0f;
    s1.cloudColor = glm::vec3(1.0f);

    layer.controlPoints.push_back({0.0f, s0});
    layer.controlPoints.push_back({1.0f, s1});

    mgr.AddLayer(layer);

    std::map<MoodParameter, float> params;
    params[MoodParameter::TimeOfDay] = 0.5f;
    mgr.Update(params);

    const auto& settings = mgr.GetBlendedSettings();

    // Logarithmic interpolation for density: exp(0.5 * (log(1e-6) + log(1))) approx 0.001
    // Actually our InterpVal handles log(max(v, 1e-6))
    // log(1e-6) = -13.8, log(1) = 0. Avg = -6.9. exp(-6.9) approx 0.001
    EXPECT_NEAR(settings.cloudDensity, 0.001f, 0.001f);

    // Oklab interpolation for color (0,0,0) to (1,1,1) at 0.5 should be around 0.125
    EXPECT_NEAR(settings.cloudColor.r, 0.128f, 0.01f); // Oklab (0.5,0,0) -> Linear approx 0.125
}

TEST(MoodManagerTest, BlendModes) {
    MoodManager mgr;

    MoodLayer base;
    base.name = "Base";
    base.priority = 0;
    base.blendMode = MoodBlendMode::Override;
    base.trackedParameter = MoodParameter::TimeOfDay;
    MoodSettings sBase = GetDefaultMoodSettings();
    sBase.cloudDensity = 0.5f;
    base.controlPoints.push_back({0.0f, sBase});
    mgr.AddLayer(base);

    MoodLayer add;
    add.name = "Add";
    add.priority = 1;
    add.blendMode = MoodBlendMode::Add;
    add.trackedParameter = MoodParameter::TimeOfDay;
    MoodSettings sAdd = GetDefaultMoodSettings();
    sAdd.cloudDensity = 0.2f;
    // Set other params to 0 for Add layer to not mess up things
    sAdd.cloudAltitude = 0.0f;
    sAdd.cloudThickness = 0.0f;
    sAdd.cloudColor = glm::vec3(0.0f);
    sAdd.cloudCoverage = 0.0f;
    sAdd.cloudSunLightScale = 0.0f;
    sAdd.cloudMoonLightScale = 0.0f;
    sAdd.cloudPowderScale = 0.0f;
    sAdd.cloudBeerPowderMix = 0.0f;
    sAdd.rayleighScale = 0.0f;
    sAdd.mieScale = 0.0f;
    sAdd.rayleighScattering = glm::vec3(0.0f);
    sAdd.mieScattering = 0.0f;
    sAdd.mieExtinction = 0.0f;

    add.controlPoints.push_back({0.0f, sAdd});
    mgr.AddLayer(add);

    std::map<MoodParameter, float> params;
    params[MoodParameter::TimeOfDay] = 0.0f;
    mgr.Update(params);

    EXPECT_NEAR(mgr.GetBlendedSettings().cloudDensity, 0.7f, 0.001f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
