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
#include <algorithm>

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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_textureWidth, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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

        void MapWidget::RenderMap(int renderWidth, int renderHeight) {
            auto weather = m_visualizer.GetWeatherManager();
            auto atmosphere = m_visualizer.GetAtmosphereManager();
            if (!weather || !atmosphere) return;

            renderWidth = std::max(256, renderWidth);
            renderHeight = std::max(256, renderHeight);

            if (m_textureWidth != renderWidth || m_textureHeight != renderHeight) {
                m_textureWidth = renderWidth;
                m_textureHeight = renderHeight;

                glBindTexture(GL_TEXTURE_2D, m_mapTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_textureWidth, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

                glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mapTexture, 0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

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
            glViewport(0, 0, m_textureWidth, m_textureHeight);
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

            m_mapShader->setFloat("uCloudCoverage", atmosphere->GetCloudCoverage());
            m_mapShader->setFloat("uCloudAltitude", atmosphere->GetCloudAltitude());
            m_mapShader->setFloat("uCloudThickness", atmosphere->GetCloudThickness());

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
            m_mapShader->setInt("uBakedWindTex", 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, weather->GetLbmScalarTexture());
            m_mapShader->setInt("uLbmScalarTex", 2);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, weather->GetLbmAerosolTexture());
            m_mapShader->setInt("uLbmAerosolTex", 3);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D_ARRAY, atmosphere->GetCloudShadowTexture());
            m_mapShader->setInt("uCloudShadowTex", 4);

            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, weather->GetLbmWindTexture());
            m_mapShader->setInt("uLbmWindTex", 5);

            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, weather->GetWindUvTexture());
            m_mapShader->setInt("uBakedWindUvTex", 6);

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

            ImGui::SetNextWindowSize(ImVec2(650, 800), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Environmental Maps", &m_show)) {

                ImGui::Combo("Map Layer", &m_selectedLayer, "Combined\0Raw Cloud Map\0Cloud Effective Coverage\0Deep Opacity Map\0LBM Simulation\0Terrain Height\0Terrain Color\0Baked Wind\0\0");

                if (ImGui::CollapsingHeader("Display Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Wind", &m_showWind); ImGui::SameLine();
                    ImGui::Checkbox("Temperature", &m_showTemperature); ImGui::SameLine();
                    ImGui::Checkbox("Humidity", &m_showHumidity);
                    ImGui::Checkbox("Pressure", &m_showPressure); ImGui::SameLine();
                    ImGui::Checkbox("Aerosol", &m_showAerosol); ImGui::SameLine();
                    ImGui::Checkbox("Cloud Cover", &m_showCloudCover);
                    ImGui::Checkbox("Cloud Height", &m_showCloudHeight); ImGui::SameLine();
                    ImGui::Checkbox("Cloud Shadow", &m_showCloudShadow);
                }

                if (ImGui::CollapsingHeader("View Controls")) {
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

                if (ImGui::CollapsingHeader("Cloud Weather Map Drawing Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto atm_mgr = m_visualizer.GetAtmosphereManager();
                    if (atm_mgr) {
                        ImGui::Checkbox("Enable Drawing Mode", &m_enableDrawing);
                        ImGui::SameLine();
                        if (ImGui::Button("Rebake Weather Map")) {
                            atm_mgr->ForceRebakeWeatherMap();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Rebake 3D Volume & Mips")) {
                            atm_mgr->RebakeWeatherMinMaxAndVolume();
                        }

                        if (m_enableDrawing) {
                            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "Drawing Mode Active: Left-click and drag on map to draw.");
                        } else {
                            ImGui::TextDisabled("Enable drawing mode above or use RMB/MMB to pan.");
                        }

                        ImGui::Combo("Draw Operation", &m_drawOp, "Erase (0)\0Steep Overwrite (1)\0Smooth Min (2)\0Smooth Max (3)\0\0");

                        ImGui::Text("Target Channel Masks (4 Layers):");
                        ImGui::Checkbox("Coverage (R)", &m_channelMask[0]); ImGui::SameLine();
                        ImGui::Checkbox("Height (G)", &m_channelMask[1]); ImGui::SameLine();
                        ImGui::Checkbox("Thickness (B)", &m_channelMask[2]); ImGui::SameLine();
                        ImGui::Checkbox("Density (A)", &m_channelMask[3]);

                        ImGui::Text("Target Channel Values:");
                        if (m_channelMask[0]) ImGui::SliderFloat("Coverage (R) Target", &m_brushTargetValue[0], 0.0f, 1.0f);
                        if (m_channelMask[1]) ImGui::SliderFloat("Height (G) Target", &m_brushTargetValue[1], 0.0f, 1.0f);
                        if (m_channelMask[2]) ImGui::SliderFloat("Thickness (B) Target", &m_brushTargetValue[2], 0.0f, 1.0f);
                        if (m_channelMask[3]) ImGui::SliderFloat("Density (A) Target", &m_brushTargetValue[3], 0.0f, 1.0f);

                        ImGui::SliderFloat("Brush Radius (World)", &m_brushRadiusWorld, 100.0f, 20000.0f);
                        ImGui::SliderFloat("Brush Strength", &m_brushStrength, 0.01f, 1.0f);
                        if (m_drawOp == 2 || m_drawOp == 3) {
                            ImGui::SliderFloat("Smoothness (k)", &m_smoothness, 0.001f, 0.5f);
                        }
                    } else {
                        ImGui::Text("AtmosphereManager is not available.");
                    }
                }

                if (ImGui::CollapsingHeader("Cloud Weather Map Management")) {
                    auto atm_mgr = m_visualizer.GetAtmosphereManager();
                    if (atm_mgr) {
                        bool is_custom = atm_mgr->IsCustomWeatherMap();
                        ImGui::Text("Active Mode: %s", is_custom ? "Custom / Painted" : "Procedural");

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

                ImVec2 avail = ImGui::GetContentRegionAvail();
                float size = std::min(avail.x, avail.y);
                if (size < 100.0f) size = 100.0f;

                // Center texture horizontally if extra space is available
                float offsetX = (avail.x - size) * 0.5f;
                if (offsetX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

                int renderSize = static_cast<int>(size);
                RenderMap(renderSize, renderSize);

                ImVec2 imagePos = ImGui::GetCursorScreenPos();
                ImGui::Image((ImTextureID)(uintptr_t)m_mapTexture, ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));

                if (ImGui::IsItemHovered()) {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0.0f) {
                        m_mapViewScale *= (wheel > 0.0f) ? 1.1f : 0.9f;
                    }

                    ImVec2 mousePos = ImGui::GetMousePos();
                    float uScreen = (mousePos.x - imagePos.x) / size;
                    float vScreen = 1.0f - (mousePos.y - imagePos.y) / size; // Flipped V for GL

                    float range = 100000.0f / m_mapViewScale;
                    glm::vec2 worldXZ = m_mapOffset + (glm::vec2(uScreen, vScreen) - glm::vec2(0.5f)) * range;

                    float worldScale = m_visualizer.GetTerrain() ? m_visualizer.GetTerrain()->GetWorldScale() : 1.0f;
                    float mapRange = 100000.0f * worldScale;
                    glm::vec2 cloudUV = worldXZ / mapRange;

                    // Toroidal wrapping
                    cloudUV.x = fmod(cloudUV.x, 1.0f); if (cloudUV.x < 0.0f) cloudUV.x += 1.0f;
                    cloudUV.y = fmod(cloudUV.y, 1.0f); if (cloudUV.y < 0.0f) cloudUV.y += 1.0f;

                    float brushRadiusUV = m_brushRadiusWorld / mapRange;

                    // Visual brush cursor overlay
                    float screenBrushRadius = (m_brushRadiusWorld / range) * size;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImU32 brushCol = m_enableDrawing ? IM_COL32(255, 220, 0, 220) : IM_COL32(200, 200, 200, 120);
                    drawList->AddCircle(mousePos, std::max(2.0f, screenBrushRadius), brushCol, 32, m_enableDrawing ? 2.0f : 1.0f);

                    // Drawing logic on Left-click drag when drawing mode is active
                    if (m_enableDrawing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        auto atm_mgr = m_visualizer.GetAtmosphereManager();
                        if (atm_mgr) {
                            glm::vec4 brushVal(m_brushTargetValue[0], m_brushTargetValue[1], m_brushTargetValue[2], m_brushTargetValue[3]);
                            if (m_drawOp == 0) { // Erase target
                                for (int c = 0; c < 4; ++c) {
                                    if (m_channelMask[c] && m_brushTargetValue[c] == 0.8f) {
                                        brushVal[c] = 0.0f;
                                    }
                                }
                            }

                            glm::vec4 maskVal(
                                m_channelMask[0] ? 1.0f : 0.0f,
                                m_channelMask[1] ? 1.0f : 0.0f,
                                m_channelMask[2] ? 1.0f : 0.0f,
                                m_channelMask[3] ? 1.0f : 0.0f
                            );

                            atm_mgr->PaintWeatherMap(
                                cloudUV,
                                brushRadiusUV,
                                brushVal,
                                maskVal,
                                m_drawOp,
                                m_brushStrength,
                                m_smoothness
                            );
                        }
                    }
                    // Panning view logic when not drawing (or RMB/MMB)
                    else if ((!m_enableDrawing && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        float worldDeltaPerPixel = range / size;
                        m_mapOffset.x -= delta.x * worldDeltaPerPixel;
                        m_mapOffset.y += delta.y * worldDeltaPerPixel;
                    }
                }
            }
            ImGui::End();
        }

    } // namespace UI
} // namespace Boidsish
