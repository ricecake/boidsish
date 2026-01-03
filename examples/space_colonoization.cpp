#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

// --- Basic Math Helper ---
struct Vec3 {
	float x, y, z;

	Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }

	Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }

	Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

	Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }

	float lengthSquared() const { return x * x + y * y + z * z; }

	float length() const { return std::sqrt(lengthSquared()); }

	Vec3 normalized() const {
		float l = length();
		return (l > 0) ? *this / l : *this;
	}
};

// --- Data Structures ---
enum class NodeType {
	INTERMEDIATE, // 1 child
	JUNCTION,     // >1 child
	TERMINAL      // 0 children (or intercepted attractor)
};

struct Node {
	int      id;
	int      parentId; // -1 if root
	Vec3     pos;
	float    weight; // Derived from limb thickness formula
	NodeType type;

	// Internal use for logic
	std::vector<int> children;
	Vec3             growthDir = {0, 0, 0};
	int              attractorCount = 0;
};

struct Edge {
	int fromNodeIdx;
	int toNodeIdx;
};

struct GraphResult {
	std::vector<Node> nodes;
	std::vector<Edge> edges;
};

struct SCConfig {
	float killDistance = 5.0f;     // Distance to delete attractor
	float influenceRadius = 20.0f; // Max distance to feel attractor
	float growthStep = 2.0f;       // Length of new segments
	bool  stopAtAttractor = true;  // If true, removes attractors (classic tree). If false, keeps growing (venation).
	float exponent = 2.0f;         // For thickness (Leonardo's rule: usually 2.0 to 3.0)
};

// --- Algorithm Class ---
class SpaceColonization {
	std::vector<Node> nodes;
	std::vector<Vec3> attractors;
	std::vector<bool> attractorActive;
	SCConfig          config;

public:
	SpaceColonization(const std::vector<Vec3>& initials, const std::vector<Vec3>& points, SCConfig cfg):
		attractors(points), config(cfg) {
		attractorActive.assign(attractors.size(), true);

		// Initialize Roots
		for (const auto& p : initials) {
			Node n;
			n.id = (int)nodes.size();
			n.parentId = -1;
			n.pos = p;
			n.weight = 1.0f;
			nodes.push_back(n);
		}
	}

	bool iterate() {
		// Reset growth vectors for all nodes
		for (auto& n : nodes) {
			n.growthDir = {0, 0, 0};
			n.attractorCount = 0;
		}

		bool growthOccurred = false;

		// 1. Associate Attractors with Nearest Nodes
		// Note: For production, use a k-d tree or octree here.
		for (size_t i = 0; i < attractors.size(); ++i) {
			if (!attractorActive[i])
				continue;

			int   closestNode = -1;
			float minDistSq = config.influenceRadius * config.influenceRadius;

			for (size_t n = 0; n < nodes.size(); ++n) {
				float distSq = (attractors[i] - nodes[n].pos).lengthSquared();
				if (distSq < minDistSq) {
					minDistSq = distSq;
					closestNode = (int)n;
				}
			}

			// Accumulate normalized vector towards attractor
			if (closestNode != -1) {
				Vec3 dir = (attractors[i] - nodes[closestNode].pos).normalized();
				nodes[closestNode].growthDir = nodes[closestNode].growthDir + dir;
				nodes[closestNode].attractorCount++;
			}
		}

		// 2. Grow New Nodes
		// We capture the current size because we are appending to the array
		size_t currentSize = nodes.size();
		for (size_t i = 0; i < currentSize; ++i) {
			if (nodes[i].attractorCount > 0) {
				Vec3 avgDir = nodes[i].growthDir.normalized();
				Node newNode;
				newNode.id = (int)nodes.size();
				newNode.parentId = (int)i;
				newNode.pos = nodes[i].pos + (avgDir * config.growthStep);
				newNode.weight = 1.0f; // Placeholder

				nodes[i].children.push_back(newNode.id);
				nodes.push_back(newNode);
				growthOccurred = true;
			}
		}

		// 3. Kill Attractors (Pruning)
		if (config.stopAtAttractor) {
			for (size_t i = 0; i < attractors.size(); ++i) {
				if (!attractorActive[i])
					continue;

				for (const auto& n : nodes) {
					if ((attractors[i] - n.pos).lengthSquared() < (config.killDistance * config.killDistance)) {
						attractorActive[i] = false;
						break;
					}
				}
			}
		}
		return growthOccurred;
	}

	// Call this after iterations finish to compute types and thickness
	GraphResult finalize() {
		GraphResult result;

		// 1. Compute Weights (Leonardo's Rule: parent area = sum of children areas)
		// We traverse backwards from end of array (leaves) to 0 (roots)
		// because children are always added after parents.
		for (int i = (int)nodes.size() - 1; i >= 0; --i) {
			if (nodes[i].children.empty()) {
				nodes[i].weight = 0.5f; // Base tip thickness
				nodes[i].type = NodeType::TERMINAL;
			} else {
				float sumPower = 0.0f;
				for (int childIdx : nodes[i].children) {
					sumPower += std::pow(nodes[childIdx].weight, config.exponent);
				}
				nodes[i].weight = std::pow(sumPower, 1.0f / config.exponent);

				if (nodes[i].children.size() > 1)
					nodes[i].type = NodeType::JUNCTION;
				else
					nodes[i].type = NodeType::INTERMEDIATE;
			}
		}

		// 2. Build Edge List
		for (const auto& n : nodes) {
			if (n.parentId != -1) {
				result.edges.push_back({n.parentId, n.id});
			}
		}

		result.nodes = nodes;
		return result;
	}
};

int main() {
	// Example Usage
	std::vector<Vec3> roots = {{0, 0, 0}};
	std::vector<Vec3> points;

	// Generate random cloud
	for (int i = 0; i < 100; ++i) {
		points.push_back({(float)(rand() % 50 - 25), (float)(rand() % 50 + 10), (float)(rand() % 50 - 25)});
	}

	SCConfig config;
	config.stopAtAttractor = true;

	SpaceColonization algo(roots, points, config);

	// Run for set iterations or until stable
	for (int i = 0; i < 5000; ++i) {
		if (!algo.iterate()) {
			break;
		}
	}

	// while(algo.iterate());

	GraphResult res = algo.finalize();
	std::cout << "Generated " << res.nodes.size() << " nodes." << std::endl;
	std::cout << "Root Thickness: " << res.nodes[0].weight << std::endl;

	return 0;
}