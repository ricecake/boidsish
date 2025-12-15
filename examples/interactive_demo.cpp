#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <chrono>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "dot.h"
#include "entity.h"
#include "graphics.h"

// A simple entity representing the player.
class PlayerEntity : public Boidsish::Entity<Boidsish::Dot> {
public:
    PlayerEntity(int id, float x, float y, float z) : Boidsish::Entity<Boidsish::Dot>(id) {
        // Initialize the shape (Dot)
        shape_ = std::make_shared<Boidsish::Dot>(id, x, y, z, 0.5f, 1.0f, 0.0f, 0.0f);
        // Set initial position in the base class
        position_ = Boidsish::Vector3(x, y, z);
    }

    // Velocity is controlled externally by the handler.
    Boidsish::Vector3 velocity;

    void UpdateEntity(
        Boidsish::EntityHandler& /* handler */,
        float /* time */,
        float /* delta_time */
    ) override {
        // Position is updated in the handler's PostTimestep.
        // This is called by the EntityHandler's main loop.
    }
};

// A handler for our interactive entities.
class InteractiveEntityHandler : public Boidsish::EntityHandler {
public:
    std::shared_ptr<PlayerEntity> player;

    // Method to create and register the player.
    std::shared_ptr<PlayerEntity> AddPlayer(float x, float y, float z) {
        int player_id = AddEntity<PlayerEntity>(x, y, z);
        player = std::dynamic_pointer_cast<PlayerEntity>(GetEntity(player_id));
        return player;
    }

protected:
    void PostTimestep(float /* time */, float delta_time) override {
        if (player) {
            // Apply player's velocity to its position.
            player->SetPosition(player->GetPosition() + player->velocity * delta_time);
        }
    }
};


int main() {
    try {
        Boidsish::Visualizer viz(1280, 720, "Interactive Demo");
        auto handler = std::make_shared<InteractiveEntityHandler>();

        auto player = handler->AddPlayer(0.0f, 0.5f, 0.0f);

        // Set the camera to track our player entity.
        viz.SetSingleTrackCamera(true, player->GetId());
        viz.SetSingleTrackDistance(10.0f);
        viz.SetSingleTrackOrbit(0.0f, 20.0f);

        // Define our custom key callback.
        viz.SetKeyCallback([&](int key, int action, int /* mods */) {
            const float speed = 5.0f;
            bool handled = false;

            if (action == GLFW_PRESS || action == GLFW_RELEASE) {
                 // Check which key was pressed.
                if (key == GLFW_KEY_W) {
                    player->velocity.z = (action == GLFW_PRESS) ? -speed : 0.0f;
                    handled = true;
                } else if (key == GLFW_KEY_S) {
                    player->velocity.z = (action == GLFW_PRESS) ? speed : 0.0f;
                    handled = true;
                } else if (key == GLFW_KEY_A) {
                    player->velocity.x = (action == GLFW_PRESS) ? -speed : 0.0f;
                    handled = true;
                } else if (key == GLFW_KEY_D) {
                    player->velocity.x = (action == GLFW_PRESS) ? speed : 0.0f;
                    handled = true;
                }
            }

            // Return true if we handled the key, false otherwise.
            // Returning true prevents the default camera controls for these keys.
            return handled;
        });

        // Add the handler itself to the visualizer.
        // This allows the visualizer to call the handler's main loop (operator()),
        // which will call PreTimestep, UpdateEntity for all entities, and PostTimestep.
        viz.AddShapeHandler(std::ref(*handler));

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
