#pragma once

#include "boidsish.h"
#include "graph.h"

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

} // namespace Boidsish
