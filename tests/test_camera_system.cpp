#include <gtest/gtest.h>
#include "graphics.h"
#include <glm/glm.hpp>
#include <memory>

using namespace Boidsish;

TEST(CameraStateTest, PushPop) {
    // We can't easily test Visualizer without a GL context,
    // but we can test the logic if we could mock things.
    // Since we don't have a mock framework set up for VisualizerImpl,
    // we'll at least verify the struct definitions and compilation.

    Camera cam;
    cam.x = 10.0f;
    cam.y = 20.0f;
    cam.z = 30.0f;

    CameraState state;
    state.camera = cam;
    state.mode = CameraMode::FREE;

    EXPECT_FLOAT_EQ(state.camera.x, 10.0f);
    EXPECT_EQ(state.mode, CameraMode::FREE);
}

// Since Visualizer requires a window/GL context, we'll skip deep integration tests
// that would fail in a headless CI environment without X11/GPU.
