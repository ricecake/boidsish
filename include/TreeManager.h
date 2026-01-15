#pragma once

#include "Tree.h"
#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

// Forward declare classes in the global namespace
class ComputeShader;
class Shader;

namespace Boidsish {

class TreeManager {
public:
    TreeManager();
    ~TreeManager();

    void update();
    void render();
    void regenerate();

    // Generation Parameters
    int   m_numAttractionPoints = 500;
    float m_attractionRadius = 20.0f;
    float m_killRadius = 5.0f;
    float m_branchLength = 1.0f;
    glm::vec3 m_canopyCenter = glm::vec3(0, 30, 0);
    float m_canopyRadius = 20.0f;
    int m_maxBranches = 5000;

private:
    void initShaders();
    void initBuffers();

    std::vector<std::unique_ptr<Tree>> m_trees;
    int m_branchCount = 0;

    std::unique_ptr<ComputeShader> m_computeShader;
    std::unique_ptr<Shader> m_renderShader;

    GLuint m_attractionPointsSSBO;
    GLuint m_treeBranchesSSBO;
    GLuint m_atomicCounterSSBO;
    GLuint m_branchGrownLockSSBO;
    GLuint m_vao;
};

} // namespace Boidsish
