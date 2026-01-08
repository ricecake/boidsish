#include <iostream>
#include <memory>
#include <vector>

#include "entity.h"
#include "graphics.h"
#include "model.h"
#include "physics.h"

class PhysicsDemoEntity: public Boidsish::Entity<Boidsish::Model> {
public:
	PhysicsDemoEntity(int id, const std::string& modelPath): Boidsish::Entity<Boidsish::Model>(id) {
		// This is a bit of a hack to load the model, as the Entity class doesn't support it directly.
		shape_ = std::make_shared<Boidsish::Model>(modelPath);
	}

	void UpdateEntity(const Boidsish::EntityHandler& handler, float time, float delta_time) override {
		// Physics will update the position
	}
};

int main(int argc, char** argv) {
	Boidsish::Visualizer     vis;
	Boidsish::PhysicsHandler physicsHandler;

	std::vector<std::shared_ptr<Boidsish::Shape>>                            shapes;
	std::vector<std::shared_ptr<PhysicsDemoEntity>>                          entities;
	std::vector<std::shared_ptr<Boidsish::PhysicsEntity<PhysicsDemoEntity>>> physicsEntities;

	// Create a dynamic cube
	auto cubeEntity = std::make_shared<PhysicsDemoEntity>(0, "assets/cube.obj");
	cubeEntity->SetPosition(0, 10, 0);
	shapes.push_back(cubeEntity->GetShape());
	entities.push_back(cubeEntity);
	physicsEntities.push_back(
		std::make_shared<Boidsish::PhysicsEntity<PhysicsDemoEntity>>(cubeEntity, physicsHandler, 1.0f)
	);

	// Create a static ground plane
	btCollisionShape*     groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
	btDefaultMotionState* groundMotionState = new btDefaultMotionState(
		btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0))
	);
	btRigidBody::btRigidBodyConstructionInfo groundRbInfo(0.0f, groundMotionState, groundShape, btVector3(0, 0, 0));
	btRigidBody*                             groundRigidBody = new btRigidBody(groundRbInfo);
	physicsHandler.addRigidBody(groundRigidBody);

	vis.AddShapeHandler([&](float time) {
		physicsHandler.update(0.016f); // Fixed timestep for now
		for (auto& pe : physicsEntities) {
			pe->update();
		}
		for (auto& e : entities) {
			e->UpdateShape();
		}
		return shapes;
	});

	vis.Run();

	return 0;
}
