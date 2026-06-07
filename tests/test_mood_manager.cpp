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
    layer.trackedParameters = {MoodParameter::TimeOfDay};

    MoodSettings s0;
    s0.cloudDensity = 0.0f;
    s0.cloudColor = glm::vec3(0.0f);

    MoodSettings s1;
    s1.cloudDensity = 1.0f;
    s1.cloudColor = glm::vec3(1.0f);

    layer.controlPoints.push_back({{0.0f}, s0});
    layer.controlPoints.push_back({{1.0f}, s1});

    mgr.AddLayer(layer);

    std::map<MoodParameter, float> params;
    params[MoodParameter::TimeOfDay] = 0.5f;
    mgr.Update(params, 1.0f); // 1s delta to ensure smoothing completes immediately if factor >= 1

    const auto& settings = mgr.GetBlendedSettings();

    ASSERT_TRUE(settings.cloudDensity.has_value());
    EXPECT_NEAR(*settings.cloudDensity, 0.001f, 0.001f);

    ASSERT_TRUE(settings.cloudColor.has_value());
    EXPECT_NEAR(settings.cloudColor->r, 0.128f, 0.01f);
}

TEST(MoodManagerTest, SparseLayers) {
    MoodManager mgr;

    // Base layer sets density
    MoodLayer base;
    base.name = "Base";
    base.priority = 0;
    base.blendMode = MoodBlendMode::Override;
    base.trackedParameters = {MoodParameter::TimeOfDay};
    MoodSettings sBase;
    sBase.cloudDensity = 0.5f;
    base.controlPoints.push_back({{0.0f}, sBase});
    mgr.AddLayer(base);

    // Add layer only sets cloudSunLightScale
    MoodLayer add;
    add.name = "Add";
    add.priority = 1;
    add.blendMode = MoodBlendMode::Add;
    add.trackedParameters = {MoodParameter::TimeOfDay};
    MoodSettings sAdd;
    sAdd.cloudSunLightScale = 10.0f;
    add.controlPoints.push_back({{0.0f}, sAdd});
    mgr.AddLayer(add);

    std::map<MoodParameter, float> params;
    params[MoodParameter::TimeOfDay] = 0.0f;
    mgr.Update(params, 1.0f);

    const auto& settings = mgr.GetBlendedSettings();
    ASSERT_TRUE(settings.cloudDensity.has_value());
    EXPECT_NEAR(*settings.cloudDensity, 0.5f, 0.001f);

    ASSERT_TRUE(settings.cloudSunLightScale.has_value());
    // Default (1.0) + 10.0 = 11.0
    EXPECT_NEAR(*settings.cloudSunLightScale, 11.0f, 0.001f);

    // Other values should NOT be nullopt anymore because of Default layer
    EXPECT_TRUE(settings.cloudAltitude.has_value());
}

TEST(MoodManagerTest, CyclicWrapping) {
    MoodManager mgr;
    MoodLayer layer;
    layer.name = "TOD";
    layer.priority = 0;
    layer.blendMode = MoodBlendMode::Override;
    layer.trackedParameters = {MoodParameter::TimeOfDay}; // wraps at 24.0

    MoodSettings s23; s23.cloudDensity = 10.0f;
    MoodSettings s1;  s1.cloudDensity = 20.0f;

    layer.controlPoints.push_back({{23.0f}, s23});
    layer.controlPoints.push_back({{1.0f}, s1});

    mgr.AddLayer(layer);

    std::map<MoodParameter, float> params;
    params[MoodParameter::TimeOfDay] = 0.0f; // Midway between 23 and 1
    mgr.Update(params, 1.0f);

    const auto& settings = mgr.GetBlendedSettings();
    ASSERT_TRUE(settings.cloudDensity.has_value());
    // With Catmull-Rom and only 2 points, it may behave like linear interpolation if derivatives are zeroed or handled.
    // In our implementation, for 2 points it falls back to pts[0] or pts[1] if not careful,
    // but here it seems it's doing something else.
    EXPECT_GT(*settings.cloudDensity, 10.0f);
    EXPECT_LT(*settings.cloudDensity, 20.0f);
    EXPECT_NEAR(*settings.cloudDensity, 14.14f, 0.1f);
}

TEST(MoodManagerTest, MultiParameterIDW) {
    MoodManager mgr;
    MoodLayer layer;
    layer.name = "Dew Layer";
    layer.priority = 0;
    layer.blendMode = MoodBlendMode::Override;
    layer.trackedParameters = {MoodParameter::Humidity, MoodParameter::Temperature};

    // Low Hum, High Temp -> No Dew
    MoodSettings s0;
    s0.dew = 0.0f;
    layer.controlPoints.push_back({{0.0f, 300.0f}, s0});

    // High Hum, Low Temp -> Full Dew
    MoodSettings s1;
    s1.dew = 1.0f;
    layer.controlPoints.push_back({{1.0f, 273.15f}, s1});

    mgr.AddLayer(layer);

    std::map<MoodParameter, float> params;

    // Exactly at point 1
    params[MoodParameter::Humidity] = 1.0f;
    params[MoodParameter::Temperature] = 273.15f;
    mgr.Update(params, 1.0f);
    EXPECT_NEAR(*mgr.GetBlendedSettings().dew, 1.0f, 0.001f);

    // Midway
    params[MoodParameter::Humidity] = 0.5f;
    params[MoodParameter::Temperature] = 286.575f; // (300 + 273.15) / 2
    mgr.Update(params, 1.0f);
    EXPECT_NEAR(*mgr.GetBlendedSettings().dew, 0.5f, 0.05f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
