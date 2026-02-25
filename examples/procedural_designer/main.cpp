#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>

#include "graphics.h"
#include "procedural_generator.h"
#include "ConfigManager.h"
#include "IWidget.h"
#include "imgui.h"

using namespace Boidsish;

class DesignerWidget : public UI::IWidget {
public:
    DesignerWidget(Visualizer& vis) : m_vis(vis) {
        strncpy(m_axiomBuf, "F", sizeof(m_axiomBuf));
        strncpy(m_rulesBuf, "F=FF-[+F+F]", sizeof(m_rulesBuf));
        m_type = 0; // Flower
        m_gridSize = 3;
        m_spacing = 5.0f;
        m_iterations = 3;
        m_seedOffset = 0;

        Generate(); // Initial generation
    }

    void Draw() override {
        if (ImGui::Begin("Procedural Designer")) {
            const char* types[] = { "Flower", "Tree" };
            ImGui::Combo("Plant Type", &m_type, types, IM_ARRAYSIZE(types));

            ImGui::InputText("Axiom", m_axiomBuf, sizeof(m_axiomBuf));

            ImGui::InputTextMultiline("Rules (one per line)", m_rulesBuf, sizeof(m_rulesBuf), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5));

            ImGui::SliderInt("Iterations", &m_iterations, 1, 6);
            ImGui::SliderInt("Grid Size", &m_gridSize, 1, 10);
            ImGui::SliderFloat("Spacing", &m_spacing, 1.0f, 20.0f);
            ImGui::InputInt("Seed Offset", &m_seedOffset);

            if (ImGui::Button("Generate")) {
                m_seedOffset += m_gridSize * m_gridSize;
                Generate();
            }
        }
        ImGui::End();
    }

private:
    void Generate() {
        m_vis.ClearShapes();

        std::string axiom = m_axiomBuf;
        std::string rulesStr = m_rulesBuf;
        std::vector<std::string> ruleList;
        std::stringstream ss(rulesStr);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                ruleList.push_back(line);
            }
        }

        for (int i = 0; i < m_gridSize; ++i) {
            for (int j = 0; j < m_gridSize; ++j) {
                unsigned int seed = i * m_gridSize + j + 12345 + m_seedOffset;
                std::shared_ptr<Model> model;
                if (m_type == 0) {
                    model = ProceduralGenerator::GenerateFlower(seed, axiom, ruleList, m_iterations);
                } else {
                    model = ProceduralGenerator::GenerateTree(seed, axiom, ruleList, m_iterations);
                }

                if (model) {
                    float x = (i - (m_gridSize - 1) * 0.5f) * m_spacing;
                    float z = (j - (m_gridSize - 1) * 0.5f) * m_spacing;
                    model->SetPosition(x, 0.0f, z);
                    m_vis.AddShape(model);
                }
            }
        }
    }

    Visualizer& m_vis;
    char m_axiomBuf[256];
    char m_rulesBuf[2048];
    int m_type;
    int m_gridSize;
    float m_spacing;
    int m_iterations;
    int m_seedOffset;
};

int main(int argc, char** argv) {
    Visualizer vis(1280, 960, "Procedural Decor Designer");

    auto& config = ConfigManager::GetInstance();
    config.SetBool("render_skybox", false);
    config.SetBool("render_terrain", false);
    config.SetBool("render_decor", false);
    config.SetBool("day_night_cycle", false);
    config.SetBool("enable_floor", true);

    auto designer = std::make_shared<DesignerWidget>(vis);
    vis.AddWidget(designer);

    // Set a reasonable initial camera position
    Camera cam = vis.GetCamera();
    cam.x = 0; cam.y = 10; cam.z = 20;
    cam.pitch = -20; cam.yaw = 0;
    vis.SetCamera(cam);

    vis.Run();

    return 0;
}
