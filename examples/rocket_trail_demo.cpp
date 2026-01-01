#include <iostream>
#include <memory>

#include "dot.h"
#include "entity.h"
#include "graphics.h"
#include "shape.h"

using namespace Boidsish;

// A simple entity that moves in a circle and has a rocket trail
class RocketEntity: public Entity<Dot> {
public:
	// Add a constructor that accepts an ID to match the AddEntity template
	RocketEntity(int id = 0): Entity<Dot>(id) {
		// Use the EntityBase setters to configure the trail
		SetTrailLength(500);
		SetTrailRocket(true);
	}

	// This pure virtual function must be implemented
	void UpdateEntity(const EntityHandler& handler, float time, float delta_time, const Visualizer* vis) override {
		// Simple circular motion based on the simulation time
		float speed = 2.0f;
		float radius = 6.0f;

		// Use the protected member position_
		position_.x = cos(time * speed) * radius;
		position_.z = sin(time * speed) * radius;
		position_.y = time / 2;
	}
};

int main() {
	try {
		Visualizer viz;

		auto&         thread_pool = viz.GetThreadPool();
		EntityHandler entity_handler(thread_pool);

		// Add the entity using the correct templated method
		entity_handler.AddEntity<RocketEntity>();

		viz.AddShapeHandler([&](float time) { return entity_handler(time); });

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
