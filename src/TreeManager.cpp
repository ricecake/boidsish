#include "TreeManager.h"
#include "TreeTypes.h"
#include <GL/glew.h>
#include "shader.h"
#include <glm/gtc/random.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace Boidsish {

TreeManager::TreeManager() : m_attractionPointsSSBO(0), m_treeBranchesSSBO(0), m_atomicCounterSSBO(0), m_branchGrownLockSSBO(0), m_vao(0) {
    initShaders();
    initBuffers();
    glGenVertexArrays(1, &m_vao);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_treeBranchesSSBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Boidsish::Branch), (void*)offsetof(Boidsish::Branch, position));
    glBindVertexArray(0);

    regenerate();

    // Pre-grow the tree so it's visible on startup
    for (int i = 0; i < 10000; ++i) {
        update();
    }
}

TreeManager::~TreeManager() {
    glDeleteBuffers(1, &m_attractionPointsSSBO);
    glDeleteBuffers(1, &m_treeBranchesSSBO);
    glDeleteBuffers(1, &m_atomicCounterSSBO);
    glDeleteBuffers(1, &m_branchGrownLockSSBO);
    glDeleteVertexArrays(1, &m_vao);
}

void TreeManager::initShaders() {
    m_computeShader = std::make_unique<ComputeShader>("shaders/tree_compute.glsl");
    m_renderShader = std::make_unique<Shader>("shaders/tree_vertex.glsl", "shaders/tree_fragment.glsl");
}

void TreeManager::initBuffers() {
    glGenBuffers(1, &m_attractionPointsSSBO);
    glGenBuffers(1, &m_treeBranchesSSBO);
    glGenBuffers(1, &m_atomicCounterSSBO);
    glGenBuffers(1, &m_branchGrownLockSSBO);
}

void TreeManager::regenerate() {
    // 1. Generate Attraction Points
    std::vector<Boidsish::AttractionPoint> attraction_points;
    attraction_points.reserve(m_numAttractionPoints);
    for (int i = 0; i < m_numAttractionPoints; ++i) {
        glm::vec3 point = glm::sphericalRand(m_canopyRadius);
        point += m_canopyCenter;
        attraction_points.push_back({glm::vec4(point, 1.0f), 1, glm::vec3(0.0f)});
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_attractionPointsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, attraction_points.size() * sizeof(Boidsish::AttractionPoint), attraction_points.data(), GL_STATIC_DRAW);

    // 2. Create Initial Trunk
    std::vector<Boidsish::Branch> branches;
    branches.push_back({glm::vec4(0, 0, 0, 1), glm::vec4(0, -m_branchLength, 0, 1), -1, 0.5f, glm::vec2(0.0f)});

    float current_y = 0;
    int parent_index = 0;
    while(current_y < m_canopyCenter.y) {
        Boidsish::Branch parent_branch = branches.back();
        current_y += m_branchLength;
        branches.push_back({glm::vec4(0, current_y, 0, 1), parent_branch.position, parent_index, 0.5f, glm::vec2(0.0f)});
        parent_index++;
    }
    m_branchCount = branches.size();

    // 3. Setup Buffers
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_treeBranchesSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_maxBranches * sizeof(Boidsish::Branch), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_branchCount * sizeof(Boidsish::Branch), branches.data());

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_atomicCounterSSBO);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &m_branchCount, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_branchGrownLockSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_maxBranches * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}

void TreeManager::update() {
    // Reset the lock buffer to all zeros every frame
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_branchGrownLockSSBO);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32I, GL_RED, GL_INT, nullptr);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Ensure clear is done before compute shader runs

    m_computeShader->use();
    m_computeShader->setInt("num_attraction_points", m_numAttractionPoints);
    m_computeShader->setFloat("kill_radius_sq", m_killRadius * m_killRadius);
    m_computeShader->setFloat("attraction_radius_sq", m_attractionRadius * m_attractionRadius);
    m_computeShader->setFloat("branch_length", m_branchLength);
    m_computeShader->setInt("max_branches", m_maxBranches);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_attractionPointsSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_treeBranchesSSBO);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, m_atomicCounterSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_branchGrownLockSSBO);

    glDispatchCompute((m_numAttractionPoints + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

    // Read back the branch count for rendering
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_atomicCounterSSBO);
    GLuint* count_ptr = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT);
    if(count_ptr) {
        m_branchCount = *count_ptr;
    }
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
}

void TreeManager::SetPosition(const glm::vec3& position) {
    this->m_position = position;
}

void TreeManager::render(const glm::mat4& view, const glm::mat4& projection) {
    m_renderShader->use();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), this->m_position);
    m_renderShader->setMat4("model", model);
    m_renderShader->setMat4("view", view);
    m_renderShader->setMat4("projection", projection);

    // Draw the tree branches
    m_renderShader->setInt("render_mode", 0); // 0 for branches
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_treeBranchesSSBO);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, m_branchCount);

    // Optionally draw the attraction points
    if (this->m_showAttractionPoints) {
        m_renderShader->setInt("render_mode", 1); // 1 for attraction points
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_attractionPointsSSBO);
        glDrawArrays(GL_POINTS, 0, m_numAttractionPoints);
    }

    glBindVertexArray(0);
}

} // namespace Boidsish
