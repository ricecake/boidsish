#include <iostream>
#include <memory>
#include <vector>

#include "ConfigManager.h"
#include "IWidget.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "weather_manager.h"

using namespace Boidsish;

class CloudDynamicsDemoWidget : public UI::IWidget {
public:
    CloudDynamicsDemoWidget(Visualizer& vis) : m_vis(vis) {}

    void Draw() override {
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Cloud Dynamics Demo")) {
            auto& postManager = m_vis.GetPostProcessingManager();
            std::shared_ptr<PostProcessing::AtmosphereEffect> atmos;

            for (auto& effect : postManager.GetPreToneMappingEffects()) {
                if (effect->GetName() == "Atmosphere") {
                    atmos = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
                    break;
                }
            }

            if (atmos) {
                auto& cdm = atmos->GetCloudDynamicsManager();

                static float radius = 800.0f;
                static float strength = 40.0f;
                static float density = -0.8f;

                ImGui::SliderFloat("Radius", &radius, 100.0f, 2000.0f);
                ImGui::SliderFloat("Strength", &strength, 0.0f, 100.0f);
                ImGui::SliderFloat("Density Offset", &density, -1.0f, 1.0f);

                auto get_target_pos = [&]() -> glm::vec3 {
                    auto screen_w = ImGui::GetIO().DisplaySize.x;
                    auto screen_h = ImGui::GetIO().DisplaySize.y;
                    auto pos = m_vis.ScreenToWorld(screen_w * 0.5f, screen_h * 0.5f);
                    return pos.value_or(m_vis.GetCamera().pos() + m_vis.GetCamera().front() * 1000.0f);
                };

                if (ImGui::Button("Spawn Sink (Push Away)")) {
                    CloudDynamicsEffect e;
                    e.position = get_target_pos();
                    e.radius = radius;
                    e.strength = strength;
                    e.densityOffset = density;
                    e.type = 0;
                    cdm.AddEffect(e);
                }

                if (ImGui::Button("Spawn Source (Flow Out)")) {
                    CloudDynamicsEffect e;
                    e.position = get_target_pos();
                    e.radius = radius;
                    e.strength = strength;
                    e.densityOffset = -density;
                    e.type = 1;
                    cdm.AddEffect(e);
                }

                ImGui::Separator();
                ImGui::Text("Instructions:");
                ImGui::TextWrapped("1. Look at the clouds.");
                ImGui::TextWrapped("2. Click 'Spawn Sink' to push clouds away from the center of the screen.");
                ImGui::TextWrapped("3. Watch how the advection field fades first, leaving a void that global wind then fills.");
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Atmosphere effect not found or disabled!");
            }
        }
        ImGui::End();
    }

private:
    Visualizer& m_vis;
};

int main(int argc, char** argv) {
    Visualizer vis(1280, 720, "Cloud Dynamics Demo");

    auto& config = ConfigManager::GetInstance();
    config.SetBool("render_terrain", true);
    config.SetBool("render_decor", false);
    config.SetBool("render_floor", true);
    config.SetFloat("cloud_coverage", 0.6f);
    config.SetFloat("cloud_density", 0.4f);

    auto demoWidget = std::make_shared<CloudDynamicsDemoWidget>(vis);
    vis.AddWidget(demoWidget);

    // Initial camera position looking at clouds
    Camera cam = vis.GetCamera();
    cam.x = 0; cam.y = 10; cam.z = 0;
    cam.pitch = 25;
    cam.yaw = 0;
    vis.SetCamera(cam);

    vis.Run();

    return 0;
}
