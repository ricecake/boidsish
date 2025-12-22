#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <map>

#include "graphics.h"
#include "entity.h"
#include "aircraft_shape.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>

using namespace Boidsish;

std::map<int, bool> key_states;

void key_callback(int key, int action, int mods) {
    if (action == GLFW_PRESS) {
        key_states[key] = true;
    } else if (action == GLFW_RELEASE) {
        key_states[key] = false;
    }
}

class AircraftEntity : public Entity<AircraftShape> {
public:
    AircraftEntity(int id) : Entity(id), speed_(20.0f) {
        SetSize(1.0f);
        SetColor(0.8f, 0.8f, 0.8f, 1.0f);
        SetPosition(0, 50, 0);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        // Flight controls
        float pitch_speed = 60.0f * delta_time;
        float roll_speed = 80.0f * delta_time;
        float yaw_speed = 40.0f * delta_time;

        if (key_states.count(GLFW_KEY_W) && key_states[GLFW_KEY_W]) {
            glm::quat pitch = glm::angleAxis(glm::radians(pitch_speed), glm::vec3(1, 0, 0));
            SetRotation(GetRotation() * pitch);
        }
        if (key_states.count(GLFW_KEY_S) && key_states[GLFW_KEY_S]) {
            glm::quat pitch = glm::angleAxis(glm::radians(-pitch_speed), glm::vec3(1, 0, 0));
            SetRotation(GetRotation() * pitch);
        }
        if (key_states.count(GLFW_KEY_A) && key_states[GLFW_KEY_A]) {
            glm::quat roll = glm::angleAxis(glm::radians(roll_speed), glm::vec3(0, 0, 1));
            SetRotation(GetRotation() * roll);
        }
        if (key_states.count(GLFW_KEY_D) && key_states[GLFW_KEY_D]) {
            glm::quat roll = glm::angleAxis(glm::radians(-roll_speed), glm::vec3(0, 0, 1));
            SetRotation(GetRotation() * roll);
        }
        if (key_states.count(GLFW_KEY_Q) && key_states[GLFW_KEY_Q]) {
            glm::quat yaw = glm::angleAxis(glm::radians(yaw_speed), glm::vec3(0, 1, 0));
            SetRotation(GetRotation() * yaw);
        }
        if (key_states.count(GLFW_KEY_E) && key_states[GLFW_KEY_E]) {
            glm::quat yaw = glm::angleAxis(glm::radians(-yaw_speed), glm::vec3(0, 1, 0));
            SetRotation(GetRotation() * yaw);
        }

        // Speed control
        if (key_states.count(GLFW_KEY_UP) && key_states[GLFW_KEY_UP]) {
            speed_ += 10.0f * delta_time;
        }
        if (key_states.count(GLFW_KEY_DOWN) && key_states[GLFW_KEY_DOWN]) {
            speed_ -= 10.0f * delta_time;
        }

        // Clamp speed
        if (speed_ < 10.0f) speed_ = 10.0f;
        if (speed_ > 100.0f) speed_ = 100.0f;

        // Update position
        glm::vec3 forward = GetRotation() * glm::vec3(0, 0, 1);
        SetPosition(GetPosition() + Vector3(forward.x, forward.y, forward.z) * speed_ * delta_time);

        // Clamp altitude
        if (GetYPos() > 150.0f) {
            SetPosition(GetXPos(), 150.0f, GetZPos());
        }
    }
private:
    float speed_;
};

class FlightHandler : public EntityHandler {
public:
    FlightHandler() {
        AddEntity<AircraftEntity>();
    }
    std::shared_ptr<AircraftEntity> GetAircraft() {
        if (GetAllEntities().empty()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<AircraftEntity>(GetAllEntities().begin()->second);
    }
};


int main() {
    try {
        Boidsish::Visualizer visualizer(1280, 720, "Flight Simulator");
        visualizer.SetManualCameraControl(false);

        visualizer.SetKeyCallback(key_callback);

        FlightHandler handler;
        visualizer.AddShapeHandler(std::ref(handler));

        while(!visualizer.ShouldClose()) {
            visualizer.Update();

            auto aircraft = handler.GetAircraft();
            if (aircraft) {
                float terrain_height = visualizer.GetTerrainHeight(aircraft->GetXPos(), aircraft->GetZPos());
                if (aircraft->GetYPos() < terrain_height + 1.0f) {
                    aircraft->SetPosition(aircraft->GetXPos(), terrain_height + 1.0f, aircraft->GetZPos());
                }

                Camera camera = visualizer.GetCamera();
                glm::vec3 aircraft_pos(aircraft->GetXPos(), aircraft->GetYPos(), aircraft->GetZPos());
                glm::quat aircraft_rot = aircraft->GetRotation();

                glm::vec3 offset = glm::vec3(0.0f, 5.0f, -15.0f);
                glm::vec3 rotated_offset = aircraft_rot * offset;

                camera.x = aircraft_pos.x + rotated_offset.x;
                camera.y = aircraft_pos.y + rotated_offset.y;
                camera.z = aircraft_pos.z + rotated_offset.z;

                glm::vec3 front = glm::normalize(aircraft_pos - glm::vec3(camera.x, camera.y, camera.z));
                camera.yaw = glm::degrees(atan2(front.x, -front.z));
                camera.pitch = glm::degrees(asin(front.y));

                visualizer.SetCamera(camera);
            }

            visualizer.Render();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
