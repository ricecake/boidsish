#include "space_colonizer.h"
#include <vector>
#include <random>

namespace Boidsish {

class MakeBranchAttractor {
private:
	std::random_device                    rd;
	std::mt19937                          eng;
	std::uniform_real_distribution<float> x;
	std::uniform_real_distribution<float> y;
	std::uniform_real_distribution<float> z;

public:
	MakeBranchAttractor(): eng(rd()), x(-1, 1), y(0, 1), z(-1, 1) {}

	Vector3 operator()(float r) {
		return std::uniform_real_distribution<float>(0, r)(eng) * Vector3(x(eng), y(eng), z(eng)).Normalized();
	}
};

static auto fruitPlacer = MakeBranchAttractor();

struct SpaceColonizer::Node {
    inline static int  count = 0;
    int                generation;
    Vector3            pos;
    Vector3            forceAccumulator;
    int                influenceCount = 0;
    int                id = count++;
    Node*              parent = nullptr;
    std::vector<Node*> children;

    Node(int gen, Vector3 p, Node* par = nullptr):
        generation(gen), pos(p), parent(par), forceAccumulator({0, 0, 0}) {}
};

SpaceColonizer::SpaceColonizer() = default;
SpaceColonizer::~SpaceColonizer() = default;

void SpaceColonizer::Initialize() {
    nodes.clear();
    nodePtrs.clear();
    fruits.clear();
    isDone = false;

    node_tree_ = std::make_unique<RTree<int, float, 3, float, 8, 4>>();
    fruit_tree_ = std::make_unique<RTree<int, float, 3, float, 8, 4>>();

    // 1. Setup Root (Trunk)
    auto root = std::make_unique<Node>(0, Vector3{0, 0, 0});
    nodePtrs.push_back(root.get());
    float pos[] = {root->pos.x, root->pos.y, root->pos.z};
    node_tree_->Insert(pos, pos, root->id);
    nodes.push_back(std::move(root));

    // 2. Setup Fruits (Mocking your fruitPlacer)
    // In your actual code, inject your 25 fruits here
    for (int i = 0; i < 25; ++i) {
        fruits.push_back(fruitPlacer(10));
        float fruit_pos[] = {fruits.back().x, fruits.back().y, fruits.back().z};
        fruit_tree_->Insert(fruit_pos, fruit_pos, i);
    }
}

bool SpaceColonizer::Step() {
    generation++;
    if (isDone || fruits.empty()) {
        isDone = true;
        return isDone;
    }

    // Reset forces
    for (auto* n : nodePtrs) {
        n->forceAccumulator = {0, 0, 0};
        n->influenceCount = 0;
    }

    bool fruitFoundNode = false;

    // 1. Associate Fruits -> Closest Node
    // This loops Fruits first, preventing explosive node checks
    for (size_t i = 0; i < fruits.size(); ++i) {
        float fruit_pos[] = {fruits[i].x, fruits[i].y, fruits[i].z};
        int closest_node_id = -1;
        node_tree_->Search(fruit_pos, fruit_pos, [&](int id) {
            closest_node_id = id;
            return false; // Stop searching after finding one
        });

        if (closest_node_id != -1) {
            Node* closest = nodes[closest_node_id].get();
            Vector3 dir = (fruits[i] - closest->pos).Normalized();
            closest->forceAccumulator += dir;
            closest->influenceCount++;
            fruitFoundNode = true;
        }
    }

    if (!fruitFoundNode) {
        isDone = true; // No fruits can reach the tree anymore
        return isDone;
    }

    // 2. Grow New Nodes
    // We capture new nodes in a temp list to avoid iterator invalidation
    std::vector<Node*> newBatch;

    for (auto* n : nodePtrs) {
        if (n->influenceCount > 0) {
            Vector3 avgDir = (n->forceAccumulator / (float)n->influenceCount).Normalized();

            Vector3 repulsion = {0, 0, 0};
            float min[] = {n->pos.x - repulsionDist, n->pos.y - repulsionDist, n->pos.z - repulsionDist};
            float max[] = {n->pos.x + repulsionDist, n->pos.y + repulsionDist, n->pos.z + repulsionDist};
            node_tree_->Search(min, max, [&](int id) {
                if (id == n->id) return true;
                const auto& other = nodes[id];
                Vector3 away = n->pos - other->pos;
                float dist = away.Magnitude();
                if (dist > 0) {
                    repulsion += (away / dist) / dist;
                }
                return true;
            });

            avgDir = (avgDir + repulsion * repulsionForce).Normalized();

            Vector3 newPos = n->pos + (avgDir * growDist);

            auto  newNode = std::make_unique<Node>(generation, newPos, n);
            Node* newNodePtr = newNode.get();
            float pos[] = {newNodePtr->pos.x, newNodePtr->pos.y, newNodePtr->pos.z};
            node_tree_->Insert(pos, pos, newNodePtr->id);

            n->children.push_back(newNodePtr); // Link Logic
            newBatch.push_back(newNodePtr);
            nodes.push_back(std::move(newNode));
        }
    }

    // Add new batch to the main pointer list
    nodePtrs.insert(nodePtrs.end(), newBatch.begin(), newBatch.end());

    // 3. Kill Fruits
    // Remove fruits that have been reached by the NEW nodes
    std::vector<int> fruits_to_remove;
    for (auto* n : newBatch) {
        float min[] = {n->pos.x - killDist, n->pos.y - killDist, n->pos.z - killDist};
        float max[] = {n->pos.x + killDist, n->pos.y + killDist, n->pos.z + killDist};
        fruit_tree_->Search(min, max, [&](int id) {
            fruits_to_remove.push_back(id);
            return true;
        });
    }

    std::sort(fruits_to_remove.begin(), fruits_to_remove.end(), std::greater<int>());
    fruits_to_remove.erase(std::unique(fruits_to_remove.begin(), fruits_to_remove.end()), fruits_to_remove.end());

    for (int id : fruits_to_remove) {
        float fruit_pos[] = {fruits[id].x, fruits[id].y, fruits[id].z};
        fruit_tree_->Remove(fruit_pos, fruit_pos, id);
        std::swap(fruits[id], fruits.back());
        fruits.pop_back();
    }
    return isDone;
}

void SpaceColonizer::TopoSortRecursive(SpaceColonizer::Node* n, std::vector<RenderNode>& out) const {
    if (!n)
        return;

    RenderNode rn;
    rn.position = n->pos;
    rn.generation = n->generation;
    rn.id = n->id;

    // Collect child positions for the "links to" list
    for (auto* child : n->children) {
        RenderNode conn;
        conn.position = child->pos;
        conn.id = child->id;
        rn.connections.push_back(conn);
    }

    out.push_back(rn);

    // Continue traversal
    for (auto* child : n->children) {
        TopoSortRecursive(child, out);
    }
}

std::vector<RenderNode> SpaceColonizer::GetTopology() const {
    std::vector<RenderNode> sortedData;
    if (!nodes.empty()) {
        TopoSortRecursive(nodes[0].get(), sortedData);
    }
    return sortedData;
}

}
