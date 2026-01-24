#include "terrain_renderer.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#include "terrain.h"
#include "graphics.h"

namespace Boidsish {

TerrainRenderer::TerrainRenderer(Visualizer* visualizer) : m_visualizer(visualizer) {
    m_shader = std::make_unique<Shader>("shaders/terrain.vert", "shaders/terrain.frag", "shaders/terrain.tcs", "shaders/terrain.tes");
    m_visualizer->SetupShaderBindings(*m_shader);
}

TerrainRenderer::~TerrainRenderer() {
    for (auto const& [key, val] : m_render_cache) {
        glDeleteVertexArrays(1, &val.vao);
        glDeleteBuffers(1, &val.vbo);
        glDeleteBuffers(1, &val.ebo);
    }
}

void TerrainRenderer::Render(const Terrain& terrain, const glm::mat4& view, const glm::mat4& projection, float tess_quality_multiplier, const std::optional<glm::vec4>& clip_plane) {
    if (terrain.vertices.empty() || terrain.indices.empty()) {
        return;
    }

    std::pair<int, int> chunk_coords = {terrain.chunk_x, terrain.chunk_z};

    ChunkRenderData render_data;
    auto it = m_render_cache.find(chunk_coords);
    if (it == m_render_cache.end()) {
        // Chunk not in cache, create new render data
        glGenVertexArrays(1, &render_data.vao);
        glGenBuffers(1, &render_data.vbo);
        glGenBuffers(1, &render_data.ebo);
        render_data.index_count = terrain.indices.size();

        glBindVertexArray(render_data.vao);

        std::vector<float> vertex_data;
        vertex_data.reserve(terrain.vertices.size() * 8);
        for (size_t i = 0; i < terrain.vertices.size(); ++i) {
            vertex_data.push_back(terrain.vertices[i].x);
            vertex_data.push_back(terrain.vertices[i].y);
            vertex_data.push_back(terrain.vertices[i].z);
            vertex_data.push_back(terrain.normals[i].x);
            vertex_data.push_back(terrain.normals[i].y);
            vertex_data.push_back(terrain.normals[i].z);
            vertex_data.push_back(0.0f); // Dummy texture coordinates
            vertex_data.push_back(0.0f);
        }

        glBindBuffer(GL_ARRAY_BUFFER, render_data.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_data.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, terrain.indices.size() * sizeof(unsigned int), terrain.indices.data(), GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // Texture coordinate attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        m_render_cache[chunk_coords] = render_data;
    } else {
        render_data = it->second;
    }

    m_shader->use();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), terrain.position);
    m_shader->setMat4("model", model);
    m_shader->setMat4("view", view);
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("uTessQualityMultiplier", tess_quality_multiplier);
    m_shader->setFloat("uTessLevelMax", 64.0f);
    m_shader->setFloat("uTessLevelMin", 1.0f);

    if (clip_plane) {
        m_shader->setVec4("clipPlane", *clip_plane);
    } else {
        m_shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0)); // No clipping
    }

    glBindVertexArray(render_data.vao);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glDrawElements(GL_PATCHES, render_data.index_count, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void TerrainRenderer::RemoveChunk(int chunk_x, int chunk_z) {
    auto it = m_render_cache.find({chunk_x, chunk_z});
    if (it != m_render_cache.end()) {
        glDeleteVertexArrays(1, &it->second.vao);
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.ebo);
        m_render_cache.erase(it);
    }
}

}
