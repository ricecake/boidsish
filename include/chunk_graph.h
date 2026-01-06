#pragma once

#include "terrain_generator.h"
#include <glm/glm.hpp>
#include <vector>
#include <map>

namespace Boidsish {

// Represents a connection between two chunks in the graph
struct ChunkEdge {
    glm::ivec2 target; // The chunk coordinate this edge connects to
    float weight;      // The cost of traversing this edge (based on altitude)
};

// Represents a node in the chunk graph
struct ChunkNode {
    glm::ivec2 pos;
    std::vector<ChunkEdge> edges;
    bool traversable = false;
};

// Custom comparator for glm::ivec2 to use in maps
struct ivec2_less {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};

class ChunkGraph {
public:
    ChunkGraph(const TerrainGenerator& terrain, int worldSizeChunks);

    void BuildGraph(float altitudeThreshold);
    std::vector<glm::ivec2> FindChunkPath(const glm::vec3& startPos, const glm::vec3& endPos);

private:
    const TerrainGenerator& _terrain;
    std::map<glm::ivec2, ChunkNode, ivec2_less> _graph;

    int _worldSizeChunks;

    // Helper methods for graph construction
    void AnalyzeChunk(int x, int z, float altitudeThreshold);
    float GetBorderLowestPoint(int x1, int z1, int x2, int z2);
    bool IsInternallyTraversable(int chunkX, int chunkZ, float altitudeThreshold);
};

} // namespace Boidsish
