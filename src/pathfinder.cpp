#include "pathfinder.h"
#include <iostream>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace Boidsish {

// Node for A* search
struct Node {
    glm::ivec2 pos; // Discretized position
    float gCost = 0.0f;
    float hCost = 0.0f;
    float fCost = 0.0f;
    glm::ivec2 parent;

    bool operator>(const Node& other) const {
        return fCost > other.fCost;
    }
};


Pathfinder::Pathfinder(const TerrainGenerator& terrain)
    : _terrain(terrain) {}

std::vector<glm::vec3> Pathfinder::findPath(const glm::vec3& start, const glm::vec3& end, const std::unordered_set<glm::ivec2, IVec2Hash>& chunkCorridor) {
    if (chunkCorridor.empty()) return {};

    glm::ivec2 startPos(static_cast<int>(start.x), static_cast<int>(start.z));
    glm::ivec2 endPos(static_cast<int>(end.x), static_cast<int>(end.z));

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openList;
    std::unordered_map<glm::ivec2, Node, IVec2Hash> allNodes;

    Node startNode;
    startNode.pos = startPos;
    startNode.hCost = glm::distance(glm::vec2(startPos), glm::vec2(endPos));
    startNode.fCost = startNode.hCost;
    openList.push(startNode);
    allNodes[startPos] = startNode;

    const float altitudePenalty = 10.0f;

    while (!openList.empty()) {
        Node currentNode = openList.top();
        openList.pop();

        if (currentNode.pos == endPos) {
            std::vector<glm::vec3> path;
            Node temp = currentNode;
            while (temp.pos != startPos) {
                float y = std::get<0>(_terrain.pointProperties(temp.pos.x, temp.pos.y));
                path.push_back(glm::vec3(temp.pos.x, y, temp.pos.y));
                temp = allNodes[temp.parent];
            }
            float startY = std::get<0>(_terrain.pointProperties(startPos.x, startPos.y));
            path.push_back(glm::vec3(startPos.x, startY, startPos.y));
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) continue;

                glm::ivec2 neighborPos = currentNode.pos + glm::ivec2(dx, dz);

                // Constrain search to the chunk corridor
                const int chunkSize = 32;
                glm::ivec2 neighborChunk = {neighborPos.x / chunkSize, neighborPos.y / chunkSize};
                if (chunkCorridor.find(neighborChunk) == chunkCorridor.end()) continue;

                float currentAltitude = std::get<0>(_terrain.pointProperties(currentNode.pos.x, currentNode.pos.y));
                float neighborAltitude = std::get<0>(_terrain.pointProperties(neighborPos.x, neighborPos.y));

                float dist = glm::distance(glm::vec2(currentNode.pos), glm::vec2(neighborPos));
                float altitudeChange = std::abs(neighborAltitude - currentAltitude);
                float moveCost = dist + altitudePenalty * altitudeChange;

                float newGCost = currentNode.gCost + moveCost;

                if (allNodes.find(neighborPos) == allNodes.end() || newGCost < allNodes[neighborPos].gCost) {
                    Node neighborNode;
                    neighborNode.pos = neighborPos;
                    neighborNode.gCost = newGCost;
                    neighborNode.hCost = glm::distance(glm::vec2(neighborPos), glm::vec2(endPos));
                    neighborNode.fCost = newGCost + neighborNode.hCost;
                    neighborNode.parent = currentNode.pos;
                    allNodes[neighborPos] = neighborNode;
                    openList.push(neighborNode);
                }
            }
        }
    }

    return std::vector<glm::vec3>(); // No path found
}

void Pathfinder::smoothPath(std::vector<glm::vec3>& path) {
    if (path.size() < 3) {
        return;
    }

    std::vector<glm::vec3> smoothedPath;
    smoothedPath.push_back(path.front());

    size_t currentIndex = 0;
    while (currentIndex < path.size() - 1) {
        size_t nextIndex = currentIndex + 1;
        for (size_t i = currentIndex + 2; i < path.size(); ++i) {
            glm::vec3 start = path[currentIndex];
            glm::vec3 end = path[i];
            glm::vec3 dir = glm::normalize(end - start);
            float dist = glm::distance(start, end);
            bool clear = true;

            for (float d = 1.0f; d < dist; d += 1.0f) {
                glm::vec3 p = start + dir * d;
                float terrainHeight = std::get<0>(_terrain.pointProperties(p.x, p.z));
                if (terrainHeight > p.y + 2.0f) { // Allow some tolerance
                    clear = false;
                    break;
                }
            }

            if (clear) {
                nextIndex = i;
            } else {
                break;
            }
        }
        smoothedPath.push_back(path[nextIndex]);
        currentIndex = nextIndex;
    }

    path = smoothedPath;
}

}
