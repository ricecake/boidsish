#include "ui/MoodWidget.h"
#include "graphics.h"
#include "mood_manager.h"
#include "imgui.h"
#include <vector>
#include <string>

namespace Boidsish {
    namespace UI {

        static const char* BlendModeToString(MoodBlendMode mode) {
            switch (mode) {
                case MoodBlendMode::Add: return "Add";
                case MoodBlendMode::Subtract: return "Subtract";
                case MoodBlendMode::Override: return "Override";
                case MoodBlendMode::Multiply: return "Multiply";
                case MoodBlendMode::Divide: return "Divide";
                default: return "Unknown";
            }
        }

        static const char* ParameterToString(MoodParameter param) {
            switch (param) {
                case MoodParameter::TimeOfDay: return "Time of Day";
                case MoodParameter::Precipitation: return "Precipitation";
                case MoodParameter::Temperature: return "Temperature";
                case MoodParameter::CloudCover: return "Cloud Cover";
                case MoodParameter::MoonAngle: return "Moon Angle";
                case MoodParameter::SunAngle: return "Sun Angle";
                case MoodParameter::MoonPhase: return "Moon Phase";
                case MoodParameter::WorldPositionX: return "World X";
                case MoodParameter::WorldPositionY: return "World Y";
                case MoodParameter::WorldPositionZ: return "World Z";
                default: return "Unknown";
            }
        }

        MoodWidget::MoodWidget(Visualizer& visualizer) : m_visualizer(visualizer) {}

        static void ShowMoodBloomSettings(const MoodBloomSettings& s) {
            if (s.toneMappingEnabled) ImGui::Text("Tone Mapping: %s", *s.toneMappingEnabled ? "Enabled" : "Disabled");
            if (s.targetLuminance) ImGui::Text("Target Luma: %.3f", *s.targetLuminance);
            if (s.whiteTemp) ImGui::Text("White Temp: %.1fK", *s.whiteTemp);
            if (s.cdlSaturation) ImGui::Text("Saturation: %.2f", *s.cdlSaturation);
            if (s.cdlSlope) ImGui::Text("CDL Slope: %.2f, %.2f, %.2f", s.cdlSlope->r, s.cdlSlope->g, s.cdlSlope->b);
        }

        static void ShowMoodSettingsDetails(const MoodSettings& s) {
            if (s.cloudDensity) ImGui::BulletText("Cloud Density: %.3f", *s.cloudDensity);
            if (s.cloudCoverage) ImGui::BulletText("Cloud Coverage: %.3f", *s.cloudCoverage);
            if (s.wetness) ImGui::BulletText("Wetness: %.3f", *s.wetness);
            if (s.dew) ImGui::BulletText("Dew: %.3f", *s.dew);
            if (s.cloudColor) {
                ImGui::BulletText("Cloud Color: ");
                ImGui::SameLine();
                glm::vec3 color = *s.cloudColor;
                ImGui::ColorEdit3("##cc", (float*)&color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            }
            if (s.rayleighScale) ImGui::BulletText("Rayleigh Scale: %.3f", *s.rayleighScale);
            if (s.mieScale) ImGui::BulletText("Mie Scale: %.3f", *s.mieScale);

            if (ImGui::TreeNode("Bloom Details")) {
                if (ImGui::TreeNode("Scene Bloom")) {
                    ShowMoodBloomSettings(s.sceneBloom);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Sky Bloom")) {
                    ShowMoodBloomSettings(s.skyBloom);
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
        }

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

            bool engineEnabled = moodMgr->IsEnabled();
            if (ImGui::Checkbox("Engine Enabled", &engineEnabled)) {
                moodMgr->SetEnabled(engineEnabled);
            }

            if (ImGui::CollapsingHeader("Environment Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto& params = moodMgr->GetCurrentParameters();
                ImGui::Columns(2, "env_params");
                for (int i = 0; i < (int)MoodParameter::Count; ++i) {
                    MoodParameter p = (MoodParameter)i;
                    ImGui::Text("%s", ParameterToString(p));
                    ImGui::NextColumn();
                    if (params.count(p)) {
                        ImGui::Text("%.3f", params.at(p));
                    } else {
                        ImGui::TextDisabled("N/A");
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& layers = moodMgr->GetLayers();
                for (size_t i = 0; i < layers.size(); ++i) {
                    auto& layer = layers[i];
                    ImGui::PushID((int)i);

                    bool layerEnabled = layer.enabled;
                    if (ImGui::Checkbox("##enabled", &layerEnabled)) {
                        layer.enabled = layerEnabled;
                    }
                    ImGui::SameLine();

                    bool active = layer.enabled && !layer.controlPoints.empty() && engineEnabled;
                    if (active) {
                        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[ACTIVE]");
                    } else {
                        ImGui::TextDisabled("[IDLE]  ");
                    }
                    ImGui::SameLine();

                    bool open = ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_SpanFullWidth, "%s (P:%d)", layer.name.c_str(), layer.priority);

                    if (open) {
                        ImGui::Text("Mode: %s", BlendModeToString(layer.blendMode));
                        std::string trackedStr;
                        for (size_t j = 0; j < layer.trackedParameters.size(); ++j) {
                            trackedStr += ParameterToString(layer.trackedParameters[j]);
                            if (j < layer.trackedParameters.size() - 1) trackedStr += ", ";
                        }
                        ImGui::Text("Tracked: %s", trackedStr.c_str());
                        ImGui::Text("Control Points: %d", (int)layer.controlPoints.size());

                        if (layer.hasLastInterpolated) {
                            if (ImGui::TreeNode("Current Contribution")) {
                                ShowMoodSettingsDetails(layer.lastInterpolated);
                                ImGui::TreePop();
                            }
                        }

                        if (ImGui::TreeNode("Interpolation Curves")) {
                            for (size_t cpIdx = 0; cpIdx < layer.controlPoints.size(); ++cpIdx) {
                                const auto& cp = layer.controlPoints[cpIdx];
                                std::string valStr;
                                for (size_t j = 0; j < cp.parameterValues.size(); ++j) {
                                    valStr += std::to_string(cp.parameterValues[j]);
                                    if (j < cp.parameterValues.size() - 1) valStr += ", ";
                                }
                                if (ImGui::TreeNode((void*)(intptr_t)cpIdx, "Values: %s", valStr.c_str())) {
                                    ShowMoodSettingsDetails(cp.settings);
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }

            bool override = moodMgr->IsOverrideEnabled();
            if (ImGui::Checkbox("User Override", &override)) {
                moodMgr->SetOverride(moodMgr->GetBlendedSettings(), override);
            }

            if (override) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Manual override active. Current settings are frozen.");
            }

            if (ImGui::CollapsingHeader("Final Blended State")) {
                const auto& settings = moodMgr->GetBlendedSettings();
                const auto& target = moodMgr->GetTargetSettings();

                if (ImGui::BeginTabBar("BlendedTabs")) {
                    if (ImGui::BeginTabItem("Smoothed")) {
                        ShowMoodSettingsDetails(settings);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Target")) {
                        ShowMoodSettingsDetails(target);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }

            ImGui::End();
        }
    }
}
