#include "ui/MoodWidget.h"
#include "graphics.h"
#include "mood_manager.h"
#include "imgui.h"

namespace Boidsish {
    namespace UI {

        MoodWidget::MoodWidget(Visualizer& visualizer) : m_visualizer(visualizer) {}

        void MoodWidget::Draw() {
            if (!ImGui::Begin("Mood Engine", &m_show)) {
                ImGui::End();
                return;
            }

            MoodManager* moodMgr = m_visualizer.GetMoodManager();
            if (!moodMgr) {
                ImGui::Text("Mood Manager not available");
                ImGui::End();
                return;
            }

            if (ImGui::CollapsingHeader("Active Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Mood Engine is active and blending layers.");
            }

            bool override = moodMgr->IsOverrideEnabled();
            if (ImGui::Checkbox("User Override", &override)) {
                moodMgr->SetOverride(moodMgr->GetBlendedSettings(), override);
            }

            if (override) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Manual override active. Current settings are frozen.");
            }

            const auto& settings = moodMgr->GetBlendedSettings();

            if (ImGui::CollapsingHeader("Atmosphere State", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (settings.cloudDensity) ImGui::Value("Cloud Density", *settings.cloudDensity);
                if (settings.cloudCoverage) ImGui::Value("Cloud Coverage", *settings.cloudCoverage);
                if (settings.cloudColor) ImGui::ColorEdit3("Cloud Color", (float*)&(*settings.cloudColor), ImGuiColorEditFlags_NoInputs);
                if (settings.rayleighScale) ImGui::Value("Rayleigh Scale", *settings.rayleighScale);
                if (settings.mieScale) ImGui::Value("Mie Scale", *settings.mieScale);
            }

            if (ImGui::CollapsingHeader("Bloom State")) {
                if (settings.sceneBloom.targetLuminance) ImGui::Text("Scene Bloom Target Luma: %.3f", *settings.sceneBloom.targetLuminance);
                if (settings.skyBloom.targetLuminance) ImGui::Text("Sky Bloom Target Luma: %.3f", *settings.skyBloom.targetLuminance);
            }

            ImGui::End();
        }
    }
}
