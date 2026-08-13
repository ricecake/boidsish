#include "ui/MapWidget.h"
#include "graphics.h"
#include "weather_manager.h"
#include "atmosphere_manager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/PostProcessingManager.h"
#include "imgui.h"
#include "shader.h"
#include "constants.h"
#include "service_locator.h"
#include "terrain_render_manager.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {
    namespace UI {

        MapWidget::MapWidget(Visualizer& visualizer) : m_visualizer(visualizer) {
            InitializeResources();
        }

        MapWidget::~MapWidget() {
            if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
            if (m_mapTexture) glDeleteTextures(1, &m_mapTexture);
            if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
            if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
        }

        void MapWidget::InitializeResources() {
            glGenFramebuffers(1, &m_fbo);
            glGenTextures(1, &m_mapTexture);
            glBindTexture(GL_TEXTURE_2D, m_mapTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mapTexture, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            m_mapShader = std::make_unique<Shader>("shaders/ui/map_viewer.vert", "shaders/ui/map_viewer.frag");

            float quadVertices[] = {
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                 1.0f, -1.0f,  1.0f, 0.0f,

                -1.0f,  1.0f,  0.0f, 1.0f,
                 1.0f, -1.0f,  1.0f, 0.0f,
                 1.0f,  1.0f,  1.0f, 1.0f
            };

            glGenVertexArrays(1, &m_quadVao);
            glGenBuffers(1, &m_quadVbo);
            glBindVertexArray(m_quadVao);
            glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glBindVertexArray(0);
        }

        void MapWidget::RenderMap() {
            auto weather = m_visualizer.GetWeatherManager();
            auto atmosphere = m_visualizer.GetAtmosphereManager();
            if (!weather || !atmosphere) return;

            std::shared_ptr<PostProcessing::AtmosphereEffect> atmos_effect;
            auto& pp_manager = m_visualizer.GetPostProcessingManager();
            for (auto& effect : pp_manager.GetPreToneMappingEffects()) {
                if (effect->GetName() == "Atmosphere") {
                    atmos_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
                    break;
                }
            }

            GLint prev_fbo;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glViewport(0, 0, 512, 512);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            m_mapShader->use();

            const auto& cam = m_visualizer.GetCamera();
            m_mapShader->setVec3("uCameraPos", cam.pos());
            m_mapShader->setVec3("uCameraFront", cam.front());
            m_mapShader->setFloat("uViewScale", m_mapViewScale);
            m_mapShader->setVec2("uViewOffset", m_mapOffset);
            float worldScale = m_visualizer.GetTerrain() ? m_visualizer.GetTerrain()->GetWorldScale() : 1.0f;
            m_mapShader->setFloat("uWorldScale", worldScale);

            m_mapShader->setInt("uSelectedLayer", m_selectedLayer);
            m_mapShader->setBool("uShowWind", m_showWind);
            m_mapShader->setBool("uShowTemperature", m_showTemperature);
            m_mapShader->setBool("uShowHumidity", m_showHumidity);
            m_mapShader->setBool("uShowPressure", m_showPressure);
            m_mapShader->setBool("uShowAerosol", m_showAerosol);
            m_mapShader->setBool("uShowCloudCover", m_showCloudCover);
            m_mapShader->setBool("uShowCloudHeight", m_showCloudHeight);
            m_mapShader->setBool("uShowCloudShadow", m_showCloudShadow);

            const auto& lbm_snap = weather->GetLatestLbmSnapshot();
            if (lbm_snap.valid) {
                m_mapShader->setIVec2("uLbmGridOrigin", lbm_snap.uboMetadata.originSize.x, lbm_snap.uboMetadata.originSize.z);
                m_mapShader->setIVec2("uLbmGridSize", lbm_snap.uboMetadata.originSize.y, lbm_snap.uboMetadata.originSize.w);
                m_mapShader->setFloat("uLbmSpacing", lbm_snap.uboMetadata.params.x);
            }

            if (atmos_effect) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, atmosphere->GetCloudWeatherTexture());
                m_mapShader->setInt("uCloudWeatherTex", 0);
            }

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, weather->GetWindTexture());
            m_mapShader->setInt("uLbmWindTex", 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, weather->GetLbmScalarTexture());
            m_mapShader->setInt("uLbmScalarTex", 2);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, weather->GetLbmAerosolTexture());
            m_mapShader->setInt("uLbmAerosolTex", 3);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D_ARRAY, atmosphere->GetCloudShadowTexture());
            m_mapShader->setInt("uCloudShadowTex", 4);

            m_mapShader->setMat4("uCloudShadowMatrix", atmosphere->GetCloudShadowMatrix());
            m_mapShader->setFloat("uTime", (float)glfwGetTime());

            auto terrain_render_mgr = ServiceLocator::Instance().Get<TerrainRenderManager>();
            if (terrain_render_mgr) {
                terrain_render_mgr->BindTerrainData(*m_mapShader);
            }

            glBindVertexArray(m_quadVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        }

        void MapWidget::Draw() {
            if (!m_show) return;

            ImGui::SetNextWindowSize(ImVec2(550, 700), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Environmental Maps", &m_show)) {

                ImGui::Combo("Map Layer", &m_selectedLayer, "Combined\0Raw Cloud Map\0Cloud Effective Coverage\0Deep Opacity Map\0LBM Simulation\0Terrain Height\0Terrain Color\0\0");

                if (ImGui::CollapsingHeader("Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Wind", &m_showWind); ImGui::SameLine();
                    ImGui::Checkbox("Temperature", &m_showTemperature); ImGui::SameLine();
                    ImGui::Checkbox("Humidity", &m_showHumidity);
                    ImGui::Checkbox("Pressure", &m_showPressure); ImGui::SameLine();
                    ImGui::Checkbox("Aerosol", &m_showAerosol); ImGui::SameLine();
                    ImGui::Checkbox("Cloud Cover", &m_showCloudCover);
                    ImGui::Checkbox("Cloud Height", &m_showCloudHeight); ImGui::SameLine();
                    ImGui::Checkbox("Cloud Shadow", &m_showCloudShadow);
                }

                if (ImGui::CollapsingHeader("View Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("Zoom", &m_mapViewScale, 0.1f, 10.0f);
                    ImGui::SliderFloat2("Offset", glm::value_ptr(m_mapOffset), -50000.0f, 50000.0f);
                    if (ImGui::Button("Reset View")) {
                        m_mapViewScale = 1.0f;
                        m_mapOffset = glm::vec2(0.0f);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Center on Camera")) {
                        const auto& cam = m_visualizer.GetCamera();
                        m_mapOffset = glm::vec2(cam.x, cam.z);
                    }
                }

                if (ImGui::CollapsingHeader("Cloud Weather Map Management")) {
                    auto atm_mgr = m_visualizer.GetAtmosphereManager();
                    if (atm_mgr) {
                        bool is_custom = atm_mgr->IsCustomWeatherMap();
                        ImGui::Text("Active Mode: %s", is_custom ? "Custom (Imported)" : "Procedural");

                        if (is_custom) {
                            if (ImGui::Button("Reset to Procedural Map")) {
                                atm_mgr->SetUseCustomWeatherMap(false);
                            }
                        }

                        ImGui::Separator();

                        // Export path and button
                        static char exportPath[256] = "assets/cloud_weather_map_export.png";
                        static std::string exportStatus = "";
                        ImGui::InputText("Export Path", exportPath, IM_ARRAYSIZE(exportPath));
                        if (ImGui::Button("Export to PNG")) {
                            if (atm_mgr->ExportCloudWeatherMap(exportPath)) {
                                exportStatus = "Exported successfully!";
                            } else {
                                exportStatus = "Export failed!";
                            }
                        }
                        if (!exportStatus.empty()) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", exportStatus.c_str());
                        }

                        ImGui::Separator();

                        // Import path and button
                        static char importPath[256] = "assets/cloud_weather_map_export.png";
                        static std::string importStatus = "";
                        ImGui::InputText("Import Path", importPath, IM_ARRAYSIZE(importPath));
                        if (ImGui::Button("Import from PNG")) {
                            if (atm_mgr->ImportCloudWeatherMap(importPath)) {
                                importStatus = "Imported successfully!";
                            } else {
                                importStatus = "Import failed!";
                            }
                        }
                        if (!importStatus.empty()) {
                            ImGui::SameLine();
                            if (importStatus.find("success") != std::string::npos) {
                                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", importStatus.c_str());
                            } else {
                                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", importStatus.c_str());
                            }
                        }
                    } else {
                        ImGui::Text("AtmosphereManager is not available.");
                    }
                }

                RenderMap();

                ImVec2 avail = ImGui::GetContentRegionAvail();
                float size = std::min(avail.x, avail.y);
                ImGui::Image((ImTextureID)(uintptr_t)m_mapTexture, ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));

                if (ImGui::IsItemHovered()) {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0.0f) {
                        m_mapViewScale *= (wheel > 0.0f) ? 1.1f : 0.9f;
                    }
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        float worldDeltaPerPixel = (100000.0f / m_mapViewScale) / size;
                        m_mapOffset.x -= delta.x * worldDeltaPerPixel;
                        m_mapOffset.y += delta.y * worldDeltaPerPixel;
                    }
                }
            }
            ImGui::End();
        }

    } // namespace UI
} // namespace Boidsish
