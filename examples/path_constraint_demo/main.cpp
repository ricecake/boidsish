#include "dot.h"
#include "entity.h"
#include "graphics.h"
#include "path.h"

// A simple entity for testing the path constraint
class ConstrainedEntity: public Boidsish::Entity<Boidsish::Dot> {
public:
	ConstrainedEntity(int id): Boidsish::Entity<Boidsish::Dot>(id) {}

	void UpdateEntity(const Boidsish::EntityHandler&, float, float) override {
		// The entity's movement is solely determined by its velocity,
		// which will be set up to challenge the path constraint.
	}
};

class PathConstraintDemoHandler: public Boidsish::EntityHandler {
public:
	PathConstraintDemoHandler(task_thread_pool::task_thread_pool& thread_pool): Boidsish::EntityHandler(thread_pool) {
		// 1. Create a path
		auto path = std::make_shared<Boidsish::Path>();
		path->AddWaypoint({-4, 0, 0});
		path->AddWaypoint({4, 0, 0});
		path->AddWaypoint({0, 0, 4});
		path->SetVisible(true);
		path->SetColor(1.0f, 0.0f, 0.0f); // Set to red

		// 2. Create an entity
		auto entity = std::make_shared<ConstrainedEntity>(0);
		entity->SetPosition(0, 0, 0);

		// Set a velocity that will move it away from the path
		entity->SetVelocity(0, 1, 0); // Moving straight up

		// 3. Set the path constraint
		float constraint_radius = 2.0f;
		entity->SetPathConstraint(path, constraint_radius);

		AddEntity(0, entity);
		paths_.push_back(path);
	}

	std::vector<std::shared_ptr<Boidsish::Shape>> operator()(float time) {
		// First, get the entity shapes from the base handler
		auto shapes = Boidsish::EntityHandler::operator()(time);

		// Then, add the path shapes
		for (const auto& path : paths_) {
			shapes.push_back(path);
		}
		return shapes;
	}

private:
	std::vector<std::shared_ptr<Boidsish::Path>> paths_;
};

int main() {
	Boidsish::Visualizer vis(800, 600, "Path Constraint Demo");

	task_thread_pool::task_thread_pool thread_pool;
	auto                               handler = std::make_shared<PathConstraintDemoHandler>(thread_pool);
	vis.AddShapeHandler([handler](float time) { return (*handler)(time); });

	vis.Run();

	return 0;
}
