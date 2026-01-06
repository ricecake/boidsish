#include "chunk_graph.h"
#include <queue>
#include <set>
#include <vector>
#include <limits>

namespace Boidsish {

ChunkGraph::ChunkGraph(const TerrainGenerator& terrain, int worldSizeChunks)
    : _terrain(terrain), _worldSizeChunks(worldSizeChunks) {}

void ChunkGraph::BuildGraph(float altitudeThreshold) {
    for (int x = 0; x < _worldSizeChunks; ++x) {
        for (int z = 0; z < _worldSizeChunks; ++z) {
            AnalyzeChunk(x, z, altitudeThreshold);
        }
    }

    // Second pass to add all edges now that traversability is known
    for (int x = 0; x < _worldSizeChunks; ++x) {
        for (int z = 0; z < _worldSizeChunks; ++z) {
            if (!_graph[{x, z}].traversable) continue;

            glm::ivec2 chunkPos = {x, z};
            const int chunkSize = 32;
            int worldX = x * chunkSize;
            int worldZ = z * chunkSize;

            // Neighbors
            glm::ivec2 neighbors[] = {{x, z - 1}, {x + 1, z}, {x, z + 1}, {x - 1, z}};
            for (const auto& neighborPos : neighbors) {
                if (neighborPos.x >= 0 && neighborPos.x < _worldSizeChunks &&
                    neighborPos.y >= 0 && neighborPos.y < _worldSizeChunks &&
                    _graph[neighborPos].traversable) {

                    float weight = 0;
                    if (neighborPos.y < z) { // North
                        weight = GetBorderLowestPoint(worldX, worldZ, worldX + chunkSize - 1, worldZ);
                    } else if (neighborPos.x > x) { // East
                        weight = GetBorderLowestPoint(worldX + chunkSize - 1, worldZ, worldX + chunkSize - 1, worldZ + chunkSize - 1);
                    } else if (neighborPos.y > z) { // South
                        weight = GetBorderLowestPoint(worldX, worldZ + chunkSize - 1, worldX + chunkSize - 1, worldZ + chunkSize - 1);
                    } else { // West
                        weight = GetBorderLowestPoint(worldX, worldZ, worldX, worldZ + chunkSize - 1);
                    }
                    _graph[chunkPos].edges.push_back({neighborPos, weight});
                }
            }
        }
    }
}

std::vector<glm::ivec2> ChunkGraph::FindChunkPath(const glm::vec3& startPos, const glm::vec3& endPos) {
    const int chunkSize = 32;
    glm::ivec2 startChunk = {static_cast<int>(startPos.x) / chunkSize, static_cast<int>(startPos.z) / chunkSize};
    glm::ivec2 endChunk = {static_cast<int>(endPos.x) / chunkSize, static_cast<int>(endPos.z) / chunkSize};

    if (_graph.find(startChunk) == _graph.end() || !_graph[startChunk].traversable ||
        _graph.find(endChunk) == _graph.end() || !_graph[endChunk].traversable) {
        return {}; // Start or end chunk is not traversable
    }

    using State = std::pair<float, glm::ivec2>;
    auto cmp = [](const State& a, const State& b) { return a.first > b.first; };
    std::priority_queue<State, std::vector<State>, decltype(cmp)> pq(cmp);

    std::map<glm::ivec2, float, ivec2_less> dist;
    std::map<glm::ivec2, glm::ivec2, ivec2_less> parent;

    for (const auto& pair : _graph) {
        dist[pair.first] = std::numeric_limits<float>::infinity();
    }

    dist[startChunk] = 0;
    pq.push({0, startChunk});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (u == endChunk) {
            std::vector<glm::ivec2> path;
            glm::ivec2 current = endChunk;
            while (current != startChunk) {
                path.push_back(current);
                current = parent[current];
            }
            path.push_back(startChunk);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (d > dist[u]) continue;

        for (const auto& edge : _graph[u].edges) {
            if (dist[u] + edge.weight < dist[edge.target]) {
                dist[edge.target] = dist[u] + edge.weight;
                parent[edge.target] = u;
                pq.push({dist[edge.target], edge.target});
            }
        }
    }

    return {}; // No path found
}

void ChunkGraph::AnalyzeChunk(int x, int z, float altitudeThreshold) {
    const int chunkSize = 32;
    glm::ivec2 chunkPos = {x, z};

    _graph[chunkPos].pos = chunkPos;
    _graph[chunkPos].traversable = IsInternallyTraversable(x, z, altitudeThreshold);
}

float ChunkGraph::GetBorderLowestPoint(int x1, int z1, int x2, int z2) {
    float lowest = std::numeric_limits<float>::max();
    int steps = std::max(std::abs(x1 - x2), std::abs(z1 - z2));
    if (steps == 0) {
        return std::get<0>(_terrain.pointProperties(x1, z1));
    }

    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float x = x1 + t * (x2 - x1);
        float z = z1 + t * (z2 - z1);
        float height = std::get<0>(_terrain.pointProperties(x, z));
        if (height < lowest) {
            lowest = height;
        }
    }
    return lowest;
}

bool ChunkGraph::IsInternallyTraversable(int chunkX, int chunkZ, float altitudeThreshold) {
    const int chunkSize = 32;
    int startX = chunkX * chunkSize;
    int startZ = chunkZ * chunkSize;

    std::queue<glm::ivec2> q;
    std::set<std::pair<int, int>> visited;

    // Seed queue with all border points below threshold
    for (int i = 0; i < chunkSize; ++i) {
        // N, S
        glm::ivec2 p_n = {startX + i, startZ};
        if (std::get<0>(_terrain.pointProperties(p_n.x, p_n.y)) < altitudeThreshold) {
            q.push(p_n); visited.insert({p_n.x, p_n.y});
        }
        glm::ivec2 p_s = {startX + i, startZ + chunkSize - 1};
        if (std::get<0>(_terrain.pointProperties(p_s.x, p_s.y)) < altitudeThreshold) {
            q.push(p_s); visited.insert({p_s.x, p_s.y});
        }
        // E, W
        glm::ivec2 p_e = {startX + chunkSize - 1, startZ + i};
        if (std::get<0>(_terrain.pointProperties(p_e.x, p_e.y)) < altitudeThreshold) {
            q.push(p_e); visited.insert({p_e.x, p_e.y});
        }
        glm::ivec2 p_w = {startX, startZ + i};
        if (std::get<0>(_terrain.pointProperties(p_w.x, p_w.y)) < altitudeThreshold) {
            q.push(p_w); visited.insert({p_w.x, p_w.y});
        }
    }

    if (q.empty()) return false;

    std::set<int> reached_borders;
    while(!q.empty()){
        glm::ivec2 current = q.front();
        q.pop();

        if (current.y == startZ) reached_borders.insert(0);
        if (current.x == startX + chunkSize - 1) reached_borders.insert(1);
        if (current.y == startZ + chunkSize - 1) reached_borders.insert(2);
        if (current.x == startX) reached_borders.insert(3);

        if (reached_borders.size() >= 2) return true;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) continue;
                glm::ivec2 n_pos = current + glm::ivec2(dx, dz);
                if (n_pos.x < startX || n_pos.x >= startX + chunkSize || n_pos.y < startZ || n_pos.y >= startZ + chunkSize) continue;
                if (visited.count({n_pos.x, n_pos.y})) continue;
                if (std::get<0>(_terrain.pointProperties(n_pos.x, n_pos.y)) < altitudeThreshold) {
                    visited.insert({n_pos.x, n_pos.y});
                    q.push(n_pos);
                }
            }
        }
    }

    return reached_borders.size() >= 2;
}

} // namespace Boidsish
