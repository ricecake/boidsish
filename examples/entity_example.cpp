#include <cmath>
#include <functional>
#include <iostream>

#include "entity.h"
#include "graphics.h"

using namespace Boidsish;

// Example entity that orbits around the origin
class OrbitalEntity: public Entity<> {
public:
	OrbitalEntity(int id, float radius, float speed, float height_offset = 0.0f):
		Entity(id), radius_(radius), speed_(speed), height_offset_(height_offset), angle_(0.0f) {
		SetSize(6.0f + radius * 0.5f);
		SetTrailLength(80);
	}

	void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
		(void)handler; // Mark unused for now
		// Update orbital angle
		angle_ += speed_ * delta_time;

		// Calculate orbital velocity using Vector3
		Vector3 orbital_velocity(
			-sin(angle_) * radius_ * speed_,
			height_offset_ * cos(time * 0.5f) * 0.5f * 1.0f, // Bobbing motion velocity with height offset
			cos(angle_) * radius_ * speed_
		);

		// Set velocity for automatic position integration
		SetVelocity(orbital_velocity);

		// Dynamic color based on angle and time
		float r = 0.5f + 0.5f * sin(angle_ + time * 0.1f);
		float g = 0.5f + 0.5f * cos(angle_ * 0.7f + time * 0.15f);
		float b = 0.5f + 0.5f * sin(angle_ * 1.3f + time * 0.2f);
		SetColor(r, g, b, 1.0f);
	}

private:
	float radius_;
	float speed_;
	float height_offset_;
	float angle_;
};

// Example entity handler that manages a swarm of orbital entities
class SwarmHandler: public EntityHandler {
public:
	SwarmHandler() {
		// Create several orbital entities with different parameters
		for (int i = 0; i < 8; ++i) {
			float radius = 3.0f + i * 0.8f;
			float speed = 0.5f + i * 0.2f;
			float height = (i % 3 - 1) * 2.0f;

			AddEntity<OrbitalEntity>(radius, speed, height);
		}
		std::cout << "Created swarm with orbital entities" << std::endl;
	}

protected:
	void PreTimestep(float time, float delta_time) override {
		(void)delta_time; // Mark unused parameter
		// Example: Add a new entity every 10 seconds
		static int last_spawn_time = 0;
		int        current_time = static_cast<int>(time);
		if (current_time > 0 && current_time % 10 == 0 && current_time != last_spawn_time) {
			float radius = 2.0f + (current_time / 10) * 0.5f;
			float speed = 1.0f;
			AddEntity<OrbitalEntity>(radius, speed, 0.0f);
			std::cout << "Spawned new entity at time " << time << std::endl;
			last_spawn_time = current_time;
		}
	}

	void PostTimestep(float time, float delta_time) override {
		// Example: Could implement collision detection, cleanup, etc.
		(void)time;
		(void)delta_time; // Suppress unused warnings
	}
};

int main() {
	try {
		// Create the visualizer
		Visualizer viz(1200, 800, "Boidsish - Entity System Example");

		// Set up camera
		Camera camera(0.0f, 3.0f, 12.0f, -15.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Create and set the entity handler
		auto handler = std::make_shared<SwarmHandler>();
		viz.AddShapeHandler(handler);

		std::cout << "Entity System Example Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera" << std::endl;
		std::cout << "  Space/Shift - Move up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  0 - Toggle auto-camera" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;
		std::cout << std::endl;
		std::cout << "Watch as entities orbit and new ones spawn every 10 seconds!" << std::endl;

		// Run the visualization
		viz.Run();

		std::cout << "Visualization ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}