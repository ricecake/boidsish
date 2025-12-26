#pragma once

#include <memory>
#include <vector>

#include <btBulletDynamicsCommon.h>

#include "entity.h"
#include "shape.h"
#include "model.h"

namespace Boidsish {

class PhysicsHandler {
public:
    PhysicsHandler();
    ~PhysicsHandler();

    void update(float deltaTime);
    void addRigidBody(btRigidBody* body);

private:
    btBroadphaseInterface* broadphase;
    btDefaultCollisionConfiguration* collisionConfiguration;
    btCollisionDispatcher* dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* dynamicsWorld;
};


template <typename T>
class PhysicsEntity {
public:
    PhysicsEntity(std::shared_ptr<T> entity, PhysicsHandler& physicsHandler, float mass = 1.0f)
        : entity_(entity), physicsHandler_(physicsHandler) {

        btCollisionShape* shape = createCollisionShape(entity->GetShape());

        btVector3 localInertia(0, 0, 0);
        if (mass != 0.0f) {
            shape->calculateLocalInertia(mass, localInertia);
        }

        btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(entity->GetXPos(), entity->GetYPos(), entity->GetZPos())));
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
        rigidBody_ = new btRigidBody(rbInfo);
        physicsHandler_.addRigidBody(rigidBody_);
    }

    ~PhysicsEntity() {
        // TODO: remove rigid body from dynamics world
    }

    void update() {
        btTransform trans;
        rigidBody_->getMotionState()->getWorldTransform(trans);

        const auto& origin = trans.getOrigin();
        entity_->SetPosition(origin.getX(), origin.getY(), origin.getZ());

        const auto& rotation = trans.getRotation();
        entity_->GetShape()->SetRotation(glm::quat(rotation.getW(), rotation.getX(), rotation.getY(), rotation.getZ()));
    }

private:
    btCollisionShape* createCollisionShape(std::shared_ptr<Shape> shape) {
        if (auto model = std::dynamic_pointer_cast<Model>(shape)) {
            // For a model, create a convex hull from its vertices
            btConvexHullShape* hull = new btConvexHullShape();
            for (const auto& mesh : model->getMeshes()) {
                for (const auto& vertex : mesh.vertices) {
                    hull->addPoint(btVector3(vertex.Position.x, vertex.Position.y, vertex.Position.z));
                }
            }
            return hull;
        } else if (auto dot = std::dynamic_pointer_cast<Dot>(shape)) {
            // For a dot, create a sphere
            return new btSphereShape(dot->GetSize() / 2.0f);
        }

        // Default to a small sphere
        return new btSphereShape(1.0f);
    }

    std::shared_ptr<T> entity_;
    PhysicsHandler& physicsHandler_;
    btRigidBody* rigidBody_;
};

} // namespace Boidsish
