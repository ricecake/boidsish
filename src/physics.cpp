#include "physics.h"

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

} // namespace Boidsish
