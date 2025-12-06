#pragma once

#include <vector>
#include <functional>

namespace Boidsish {

// Structure representing a single dot/particle
struct Dot {
    int id;                  // Unique identifier for trail tracking
    float x, y, z;           // Position in 3D space
    float size;              // Size of the dot
    float r, g, b, a;        // Color (RGBA)
    int trail_length;        // Number of trail segments to maintain

    Dot(int id = 0, float x = 0.0f, float y = 0.0f, float z = 0.0f,
        float size = 1.0f,
        float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f,
        int trail_length = 10)
        : id(id), x(x), y(y), z(z), size(size), r(r), g(g), b(b), a(a), trail_length(trail_length) {}
};// Function type for user-defined dot generation
using DotFunction = std::function<std::vector<Dot>(float time)>;

// Camera structure for 3D view control
struct Camera {
    float x, y, z;           // Camera position
    float pitch, yaw;        // Camera rotation
    float fov;               // Field of view

    Camera(float x = 0.0f, float y = 0.0f, float z = 5.0f,
           float pitch = 0.0f, float yaw = 0.0f, float fov = 45.0f)
        : x(x), y(y), z(z), pitch(pitch), yaw(yaw), fov(fov) {}
};

// Main visualization class
class Visualizer {
public:
    Visualizer(int width = 800, int height = 600, const char* title = "Boidsish 3D Visualizer");
    ~Visualizer();

    // Set the function that generates dots for each frame
    void SetDotFunction(DotFunction func);

    // Start the visualization loop
    void Run();

    // Check if the window should close
    bool ShouldClose() const;

    // Update one frame
    void Update();

    // Render one frame
    void Render();

    // Get current camera
    Camera& GetCamera();

    // Set camera position and orientation
    void SetCamera(const Camera& camera);

private:
    struct VisualizerImpl;
    VisualizerImpl* impl;
};

} // namespace Boidsish