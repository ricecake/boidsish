#pragma once

#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "TreeTypes.h"

// Forward declare classes in the global namespace
class ComputeShader;
class Shader;

namespace Boidsish {

class TreeManager {
public:
    TreeManager();
    ~TreeManager();

    void update();
    void render(const glm::mat4& view, const glm::mat4& projection);
    void regenerate();
    void SetPosition(const glm::vec3& position);
    void SetScale(float scale);

    // Getters
    int GetNumAttractionPoints() const { return m_numAttractionPoints; }
    float GetAttractionRadius() const { return m_attractionRadius; }
    float GetKillRadius() const { return m_killRadius; }
    float GetBranchLength() const { return m_branchLength; }
    const glm::vec3& GetCanopyCenter() const { return m_canopyCenter; }
    float GetCanopyRadius() const { return m_canopyRadius; }
    int GetMaxBranches() const { return m_maxBranches; }
    bool GetShowAttractionPoints() const { return m_showAttractionPoints; }

    // Setters
    void SetNumAttractionPoints(int value) { m_numAttractionPoints = value; }
    void SetAttractionRadius(float value) { m_attractionRadius = value; }
    void SetKillRadius(float value) { m_killRadius = value; }
    void SetBranchLength(float value) { m_branchLength = value; }
    void SetCanopyCenter(const glm::vec3& value) { m_canopyCenter = value; }
    void SetCanopyRadius(float value) { m_canopyRadius = value; }
    void SetMaxBranches(int value) { m_maxBranches = value; }
    void SetShowAttractionPoints(bool value) { m_showAttractionPoints = value; }

private:
    // Generation Parameters
    int   m_numAttractionPoints = 1000;
    float m_attractionRadius = 8.0f;
    float m_killRadius = 6.0f;
    float m_branchLength = 1.0f;
    glm::vec3 m_canopyCenter = glm::vec3(0, 30, 0);
    float m_canopyRadius = 20.0f;
    int m_maxBranches = 5000;
    bool m_showAttractionPoints = false;

    glm::vec3 m_position = glm::vec3(0.0f);
    float m_scale = 1.0f;
    void initShaders();
    void initBuffers();

    int m_branchCount = 0;

    std::unique_ptr<ComputeShader> m_computeShader;
    std::unique_ptr<Shader> m_renderShader;

    GLuint m_attractionPointsSSBO;
    GLuint m_treeBranchesSSBO;
    GLuint m_atomicCounterSSBO;
    GLuint m_branchGrownLockSSBO;
    GLuint m_vao;
    GLuint m_attractionPointVao;
};

} // namespace Boidsish
