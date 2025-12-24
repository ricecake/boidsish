#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>

#include "graphics.h"
#include "arrow.h"
#include <GLFW/glfw3.h>

struct FighterPart {
    std::shared_ptr<Boidsish::Shape> shape;
    glm::vec3 initial_position;
    glm::quat initial_rotation;
};

// Function to create a simple fighter jet model
std::vector<FighterPart> CreateFighter() {
    std::vector<FighterPart> fighter_parts;

    // Fuselage
    auto fuselage_shape = std::make_shared<Boidsish::Arrow>(0, 0, 0, 0, 0.6f, 0.15f, 0.1f, 0.5f, 0.5f, 0.5f);
    glm::vec3 fuselage_pos = glm::vec3(0.0f);
    glm::quat fuselage_rot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    fuselage_shape->SetPosition(fuselage_pos.x, fuselage_pos.y, fuselage_pos.z);
    fuselage_shape->SetRotation(fuselage_rot);
    fighter_parts.push_back({fuselage_shape, fuselage_pos, fuselage_rot});

    // Wings
    auto left_wing_shape = std::make_shared<Boidsish::Arrow>(0, 0, 0, 0, 0.4f, 0.1f, 0.05f, 0.7f, 0.7f, 0.7f);
    glm::vec3 left_wing_pos = glm::vec3(-0.5f, 0.0f, 0.0f);
    glm::quat left_wing_rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    left_wing_shape->SetPosition(left_wing_pos.x, left_wing_pos.y, left_wing_pos.z);
    left_wing_shape->SetRotation(left_wing_rot);
    fighter_parts.push_back({left_wing_shape, left_wing_pos, left_wing_rot});

    auto right_wing_shape = std::make_shared<Boidsish::Arrow>(0, 0, 0, 0, 0.4f, 0.1f, 0.05f, 0.7f, 0.7f, 0.7f);
    glm::vec3 right_wing_pos = glm::vec3(0.5f, 0.0f, 0.0f);
    glm::quat right_wing_rot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    right_wing_shape->SetPosition(right_wing_pos.x, right_wing_pos.y, right_wing_pos.z);
    right_wing_shape->SetRotation(right_wing_rot);
    fighter_parts.push_back({right_wing_shape, right_wing_pos, right_wing_rot});

    // Tail
    auto tail_shape = std::make_shared<Boidsish::Arrow>(0, 0, 0, 0, 0.3f, 0.08f, 0.04f, 0.6f, 0.6f, 0.6f);
    glm::vec3 tail_pos = glm::vec3(0.0f, 0.2f, -0.4f);
    glm::quat tail_rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // No rotation
    tail_shape->SetPosition(tail_pos.x, tail_pos.y, tail_pos.z);
    tail_shape->SetRotation(tail_rot);
    fighter_parts.push_back({tail_shape, tail_pos, tail_rot});

    return fighter_parts;
}

namespace {
    // Constants for flight dynamics
    constexpr float kMinSpeed = 1.0f;
    constexpr float kMaxSpeed = 50.0f;
    constexpr float kSpeedStep = 2.0f;
    constexpr float kMouseSensitivity = 0.1f;
    constexpr float kRollFactor = 2.0f;
    constexpr float kRollSmoothing = 5.0f;
}

// Custom input handler for flight dynamics
void FighterInputHandler(Boidsish::Visualizer& visualizer, const Boidsish::InputState& input) {
    auto& camera = visualizer.GetCamera();

    // Adjust speed with '[' and ']' keys
    if (input.keys[GLFW_KEY_LEFT_BRACKET]) {
        camera.speed = std::max(kMinSpeed, camera.speed - kSpeedStep * input.delta_time);
    }
    if (input.keys[GLFW_KEY_RIGHT_BRACKET]) {
        camera.speed = std::min(kMaxSpeed, camera.speed + kSpeedStep * input.delta_time);
    }

    // Constant forward motion
    glm::vec3 front = camera.front();
    camera.x += front.x * camera.speed * input.delta_time;
    camera.y += front.y * camera.speed * input.delta_time;
    camera.z += front.z * camera.speed * input.delta_time;

    // Update camera orientation based on mouse movement
    camera.yaw += input.mouse_delta_x * kMouseSensitivity;
    camera.pitch -= input.mouse_delta_y * kMouseSensitivity;

    // Clamp pitch to avoid flipping
    camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);

    // Calculate roll for banking effect
    float target_roll = -input.mouse_delta_x * kRollFactor;
    // Smoothly interpolate to the target roll and back to zero
    camera.roll = glm::mix(camera.roll, target_roll, input.delta_time * kRollSmoothing);
}


int main() {
    try {
        Boidsish::Visualizer visualizer(1280, 720, "Fighter Demo");

        // Create the fighter model once
        auto fighter_parts = CreateFighter();

        // Set the custom input handler
        visualizer.SetInputCallback(
            [&](const Boidsish::InputState& input) {
                FighterInputHandler(visualizer, input);
            }
        );

        // Shape handler to draw the fighter model
        visualizer.AddShapeHandler([&fighter_parts, &visualizer](float) {
            auto camera = visualizer.GetCamera();

            // Position the fighter in front of the camera
            glm::vec3 model_pos = camera.pos() + camera.front() * 3.0f - camera.up() * 0.5f;

            // Create a combined orientation from the camera's rotation
            glm::quat orientation = glm::quat(glm::vec3(glm::radians(camera.pitch), glm::radians(camera.yaw), glm::radians(camera.roll)));

            std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
            for (auto& part : fighter_parts) {
                // Apply the camera's orientation to the part's initial rotation
                part.shape->SetRotation(orientation * part.initial_rotation);
                // Position each part relative to the model's center in world space
                glm::vec3 final_pos = model_pos + orientation * part.initial_position;
                part.shape->SetPosition(final_pos.x, final_pos.y, final_pos.z);
                shapes.push_back(part.shape);
            }

            return shapes;
        });

        visualizer.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
