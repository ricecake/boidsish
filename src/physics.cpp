#include "physics.h"
#include "entity.h"

namespace Boidsish {

	PhysicsHandler::PhysicsHandler() {
		// Build the broadphase
		broadphase = new btDbvtBroadphase();

		// Set up the collision configuration and dispatcher
		collisionConfiguration = new btDefaultCollisionConfiguration();
		dispatcher = new btCollisionDispatcher(collisionConfiguration);

		// The actual physics solver
		solver = new btSequentialImpulseConstraintSolver;

		// The world.
		dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
		dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
	}

	PhysicsHandler::~PhysicsHandler() {
		delete dynamicsWorld;
		delete solver;
		delete dispatcher;
		delete collisionConfiguration;
		delete broadphase;
	}

	void PhysicsHandler::update(float deltaTime) {
		dynamicsWorld->stepSimulation(deltaTime, 10);
	}

	void PhysicsHandler::addRigidBody(btRigidBody* body) {
		dynamicsWorld->addRigidBody(body);
	}

	void PhysicsHandler::removeRigidBody(btRigidBody* body) {
		dynamicsWorld->removeRigidBody(body);
	}

	EntityBase* PhysicsHandler::rayIntersects(const btVector3& from, const btVector3& to, float radius) {
		btSphereShape sphere(radius);
		btTransform start, end;
		start.setIdentity();
		end.setIdentity();
		start.setOrigin(from);
		end.setOrigin(to);

		btCollisionWorld::ClosestConvexResultCallback callback(from, to);
		dynamicsWorld->convexSweepTest(&sphere, start, end, callback);

		if (callback.hasHit()) {
			return static_cast<EntityBase*>(callback.m_hitCollisionObject->getUserPointer());
		}

		return nullptr;
	}

} // namespace Boidsish
