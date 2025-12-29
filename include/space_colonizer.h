#pragma once

#include <vector>
#include <memory>
#include "vector.h"
#include <RTree.h>

namespace Boidsish {

struct RenderNode {
    int                     generation;
    int                     id;
    Vector3                 position;
    std::vector<RenderNode> connections; // The "list of nodes to link to"
};

class SpaceColonizer {
private:
    struct Node; // Forward declaration for internal implementation

    std::vector<std::unique_ptr<Node>> nodes;    // Owner of memory
    std::vector<Node*>                 nodePtrs; // Quick access for iteration
    std::vector<Vector3>               fruits;

    // R-Trees for spatial indexing
    std::unique_ptr<RTree<Node*, float, 3, float, 8, 4>> node_tree_;
    std::unique_ptr<RTree<int, float, 3, float, 8, 4>> fruit_tree_;

    // Config
    float killDist = 2.0f;    // Distance to delete fruit
    float detectDist = 20.0f; // Radius of influence
    float growDist = 1.0f;    // Length of new branch
    float repulsionDist = 1.5f; // Radius for edge avoidance
    float repulsionForce = 1.0f; // Strength of edge avoidance
    int   node_id = 0;

public:
    bool isDone = false;
    int  generation = 0;

    SpaceColonizer();
    ~SpaceColonizer();

    void Initialize();
    bool Step();
    std::vector<RenderNode> GetTopology() const;

    bool IsFinished() const { return isDone; }

    void AddFruit(Vector3 pos) { fruits.push_back(pos); }

private:
    void TopoSortRecursive(SpaceColonizer::Node* n, std::vector<RenderNode>& out) const;
};

}