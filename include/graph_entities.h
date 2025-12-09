#pragma once

#include "boidsish.h"
#include "graph.h"
#include "collision_shapes.h"

namespace Boidsish {

// A proxy entity to represent a single vertex of a Graph for collision purposes.
// This entity is static and does not update its own position.
class GraphVertexEntity : public Entity {
public:
    GraphVertexEntity(int id, const Graph::Vertex& vertex, std::shared_ptr<Graph> parent_graph)
        : Entity(id), parent_graph_(parent_graph) {
        SetPosition(vertex.position);
        SetSize(vertex.size);
        float r, g, b, a;
        GetColor(r, g, b, a);
        SetColor(vertex.r, vertex.g, vertex.b, vertex.a);
        SetTrailLength(0);
    }

    // This entity is static, so its update function does nothing.
    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler;
        (void)time;
        (void)delta_time;
    }

    std::shared_ptr<Graph> GetParentGraph() const {
        return parent_graph_;
    }

private:
    std::shared_ptr<Graph> parent_graph_;
};

// A proxy entity to represent a single edge of a Graph for collision purposes.
class GraphEdgeEntity : public Entity {
public:
    GraphEdgeEntity(int id, const Graph::Vertex& v1, const Graph::Vertex& v2, std::shared_ptr<Graph> parent_graph)
        : Entity(id), parent_graph_(parent_graph) {

        // The size of an edge entity is its radius for collision purposes.
        // We'll average the size of the two vertices.
        float radius = (v1.size + v2.size) * 0.5f;
        SetSize(radius);

        // The position is the midpoint, though it's less critical for a capsule.
        SetPosition((v1.position + v2.position) * 0.5f);

        // Store the capsule geometry
        capsule_ = {v1.position, v2.position, radius};

        // Edges are not rendered directly and have no trail
        SetTrailLength(0);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler; (void)time; (void)delta_time;
    }

    CollisionShapeType GetCollisionShapeType() const override {
        return CollisionShapeType::CAPSULE;
    }

    Capsule GetCapsule() const {
        return capsule_;
    }

    std::shared_ptr<Graph> GetParentGraph() const {
        return parent_graph_;
    }

private:
    Capsule capsule_;
    std::shared_ptr<Graph> parent_graph_;
};

} // namespace Boidsish
