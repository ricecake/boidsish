#pragma once

#include <vector>
#include <functional>
#include <map>
#include <memory>

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
};

// Function type for user-defined dot generation
using DotFunction = std::function<std::vector<Dot>(float time)>;

// Base entity class for the entity system
class Entity {
public:
    Entity(int id = 0) : id_(id), position_{0.0f, 0.0f, 0.0f}, velocity_{0.0f, 0.0f, 0.0f},
                         size_(8.0f), color_{1.0f, 1.0f, 1.0f, 1.0f}, trail_length_(50) {}
    virtual ~Entity() = default;

    // Called each frame to update the entity
    virtual void UpdateEntity(float time, float delta_time) = 0;

    // Getters and setters
    int GetId() const { return id_; }

    // Position (used as motion vector by EntityHandler)
    float GetX() const { return position_[0]; }
    float GetY() const { return position_[1]; }
    float GetZ() const { return position_[2]; }
    void SetPosition(float x, float y, float z) { position_[0] = x; position_[1] = y; position_[2] = z; }

    // Velocity (motion vector per frame)
    float GetVelX() const { return velocity_[0]; }
    float GetVelY() const { return velocity_[1]; }
    float GetVelZ() const { return velocity_[2]; }
    void SetVelocity(float vx, float vy, float vz) { velocity_[0] = vx; velocity_[1] = vy; velocity_[2] = vz; }

    // Visual properties
    float GetSize() const { return size_; }
    void SetSize(float size) { size_ = size; }

    void GetColor(float& r, float& g, float& b, float& a) const {
        r = color_[0]; g = color_[1]; b = color_[2]; a = color_[3];
    }
    void SetColor(float r, float g, float b, float a = 1.0f) {
        color_[0] = r; color_[1] = g; color_[2] = b; color_[3] = a;
    }

    int GetTrailLength() const { return trail_length_; }
    void SetTrailLength(int length) { trail_length_ = length; }

protected:
    int id_;
    float position_[3];      // Current position (treated as motion vector)
    float velocity_[3];      // Velocity per frame
    float size_;
    float color_[4];         // RGBA
    int trail_length_;
};

// Entity handler that manages entities and provides dot generation
class EntityHandler {
public:
    EntityHandler() : last_time_(-1.0f), next_id_(0) {}
    virtual ~EntityHandler() = default;

    // Operator() to make this compatible with DotFunction
    std::vector<Dot> operator()(float time);

    // Entity management
    template<typename T, typename... Args>
    int AddEntity(Args&&... args) {
        int id = next_id_++;
        entities_[id] = std::make_unique<T>(id, std::forward<Args>(args)...);
        return id;
    }

    void RemoveEntity(int id) {
        entities_.erase(id);
    }

    Entity* GetEntity(int id) {
        auto it = entities_.find(id);
        return (it != entities_.end()) ? it->second.get() : nullptr;
    }

protected:
    // Override these for custom behavior
    virtual void PreTimestep(float time, float delta_time) {}
    virtual void PostTimestep(float time, float delta_time) {}

private:
    std::map<int, std::unique_ptr<Entity>> entities_;
    std::map<int, float[3]> entity_positions_; // Absolute positions for integration
    float last_time_;
    int next_id_;
};

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

    // Set the function/handler that generates dots for each frame
    void SetDotHandler(DotFunction func);

    // Legacy method name for compatibility
    void SetDotFunction(DotFunction func) { SetDotHandler(func); }

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