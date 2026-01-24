#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>
#include <optional>

#include "shader.h"
#include <glm/glm.hpp>

namespace Boidsish {

class Terrain;
class Visualizer;

// A hasher for std::pair<int, int> to be used in std::unordered_map
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

class TerrainRenderer {
public:
    TerrainRenderer(Visualizer* visualizer);
    ~TerrainRenderer();

    void Render(const Terrain& terrain, const glm::mat4& view, const glm::mat4& projection, float tess_quality_multiplier, const std::optional<glm::vec4>& clip_plane);
    void RemoveChunk(int chunk_x, int chunk_z);

private:
    struct ChunkRenderData {
        unsigned int vao, vbo, ebo;
        size_t index_count;
    };

    Visualizer* m_visualizer;
    std::unique_ptr<Shader> m_shader;
    std::unordered_map<std::pair<int, int>, ChunkRenderData, pair_hash> m_render_cache;
};

}
