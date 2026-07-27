#include "ui/RenderWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "light_manager.h"
#include "ui/AmbientSettingsComponent.h"
#include "ui/LightsComponent.h"
#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"

namespace Boidsish {
	namespace UI {

		RenderWidget::RenderWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void RenderWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 640), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Render", &m_show)) {
				// 1. Rendering Settings (from ConfigWidget)
				if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
					float render_scale = m_visualizer.GetRenderScale();
					if (ImGui::SliderFloat("Render Scale", &render_scale, 0.1f, 1.0f)) {
						m_visualizer.SetRenderScale(render_scale);
					}

					auto& config_manager = ConfigManager::GetInstance();
					bool  enable_shadows = config_manager.GetAppSettingBool("enable_shadows", true);
					if (ImGui::Checkbox("Enable Shadows", &enable_shadows)) {
						config_manager.SetBool("enable_shadows", enable_shadows);
					}
				}

				// 2. Mesh Optimization (from ConfigWidget)
				if (ImGui::CollapsingHeader("Mesh Optimization", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config_manager = ConfigManager::GetInstance();

					bool mesh_opt = config_manager.GetAppSettingBool("mesh_optimizer_enabled", true);
					if (ImGui::Checkbox("Enable Optimizer", &mesh_opt)) {
						config_manager.SetBool("mesh_optimizer_enabled", mesh_opt);
					}

					bool mesh_simp = config_manager.GetAppSettingBool("mesh_simplifier_enabled", true);
					if (ImGui::Checkbox("Enable Simplifier", &mesh_simp)) {
						config_manager.SetBool("mesh_simplifier_enabled", mesh_simp);
					}

					if (mesh_simp) {
						float ratio = config_manager.GetAppSettingFloat("mesh_simplifier_target_ratio", 0.0f);
						if (ImGui::SliderFloat("Global Ratio Limit", &ratio, 0.00f, 1.0f)) {
							config_manager.SetFloat("mesh_simplifier_target_ratio", ratio);
						}

						ImGui::Separator();
						ImGui::Text("Pre-build Assets");
						float err_pb = config_manager.GetAppSettingFloat("mesh_simplifier_error_prebuild", 0.01f);
						if (ImGui::SliderFloat("Error Rate (Pre-build)", &err_pb, 0.001f, 0.2f, "%.3f")) {
							config_manager.SetFloat("mesh_simplifier_error_prebuild", err_pb);
						}

						int         agg_pb = config_manager.GetAppSettingInt("mesh_simplifier_aggression_prebuild", 0);
						const char* agg_levels[] = {"Low", "Medium", "High"};
						int         current_agg_pb = (agg_pb == 40) ? 2 : (agg_pb == 8 ? 1 : 0);
						if (ImGui::Combo("Aggression (Pre-build)", &current_agg_pb, agg_levels, 3)) {
							int new_agg = (current_agg_pb == 2) ? 40 : (current_agg_pb == 1 ? 8 : 0);
							config_manager.SetInt("mesh_simplifier_aggression_prebuild", new_agg);
						}

						ImGui::Separator();
						ImGui::Text("Procedural Models");
						float err_proc = config_manager.GetAppSettingFloat("mesh_simplifier_error_procedural", 0.05f);
						if (ImGui::SliderFloat("Error Rate (Procedural)", &err_proc, 0.001f, 0.2f, "%.3f")) {
							config_manager.SetFloat("mesh_simplifier_error_procedural", err_proc);
						}

						int agg_proc = config_manager.GetAppSettingInt("mesh_simplifier_aggression_procedural", 40);
						int current_agg_proc = (agg_proc == 40) ? 2 : (agg_proc == 8 ? 1 : 0);
						if (ImGui::Combo("Aggression (Procedural)", &current_agg_proc, agg_levels, 3)) {
							int new_agg = (current_agg_proc == 2) ? 40 : (current_agg_proc == 1 ? 8 : 0);
							config_manager.SetInt("mesh_simplifier_aggression_procedural", new_agg);
						}
					}
				}

				// 2.5 Cloud Quality (New section)
				if (ImGui::CollapsingHeader("Cloud Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& manager = m_visualizer.GetPostProcessingManager();
					std::shared_ptr<PostProcessing::AtmosphereEffect> atmosphere_effect = nullptr;
					for (auto& effect : manager.GetPreToneMappingEffects()) {
						if (effect->GetName() == "Atmosphere") {
							atmosphere_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
							break;
						}
					}

					if (atmosphere_effect) {
						ImGui::Text("Raymarching");
						float max_dist = atmosphere_effect->GetCloudMaxRayDistance();
						if (ImGui::SliderFloat("Max Ray Distance", &max_dist, 10000.0f, 200000.0f, "%.0f")) {
							atmosphere_effect->SetCloudMaxRayDistance(max_dist);
						}

						int min_samples = atmosphere_effect->GetCloudMinSamples();
						if (ImGui::SliderInt("Min Samples", &min_samples, 4, 128)) {
							atmosphere_effect->SetCloudMinSamples(min_samples);
						}

						int max_samples = atmosphere_effect->GetCloudMaxSamples();
						if (ImGui::SliderInt("Max Samples", &max_samples, 16, 256)) {
							atmosphere_effect->SetCloudMaxSamples(max_samples);
						}

						ImGui::Separator();
						ImGui::Text("Temporal Accumulation");
						float gamma = atmosphere_effect->GetCloudTemporalGamma();
						if (ImGui::SliderFloat("Temporal Gamma", &gamma, 0.1f, 10.0f, "%.2f")) {
							atmosphere_effect->SetCloudTemporalGamma(gamma);
						}

						float max_history = atmosphere_effect->GetCloudMaxHistoryLength();
						if (ImGui::SliderFloat("Max History Length", &max_history, 1.0f, 256.0f, "%.0f")) {
							atmosphere_effect->SetCloudMaxHistoryLength(max_history);
						}

						bool enable_temporal = atmosphere_effect->GetCloudEnableTemporal();
						if (ImGui::Checkbox("Enable TAA Accumulation", &enable_temporal)) {
							atmosphere_effect->SetCloudEnableTemporal(enable_temporal);
						}

						ImGui::Separator();
						ImGui::Text("SVGF (Spatial Filter)");

						bool enable_spatial = atmosphere_effect->GetCloudEnableSpatialFilter();
						if (ImGui::Checkbox("Enable SVGF", &enable_spatial)) {
							atmosphere_effect->SetCloudEnableSpatialFilter(enable_spatial);
						}

						int svgf_passes = atmosphere_effect->GetCloudSvgfPasses();
						if (ImGui::SliderInt("SVGF Passes", &svgf_passes, 0, 6)) {
							atmosphere_effect->SetCloudSvgfPasses(svgf_passes);
						}

						float phi_luma = atmosphere_effect->GetCloudPhiLuma();
						if (ImGui::SliderFloat("Phi Luma", &phi_luma, 0.0f, 100.0f, "%.1f")) {
							atmosphere_effect->SetCloudPhiLuma(phi_luma);
						}

						float phi_depth = atmosphere_effect->GetCloudPhiDepth();
						if (ImGui::SliderFloat("Phi Depth", &phi_depth, 0.0f, 10.0f, "%.2f")) {
							atmosphere_effect->SetCloudPhiDepth(phi_depth);
						}

						float phi_density = atmosphere_effect->GetCloudPhiDensity();
						if (ImGui::SliderFloat("Phi Density", &phi_density, 0.0f, 2.0f, "%.3f")) {
							atmosphere_effect->SetCloudPhiDensity(phi_density);
						}

						float history_boost = atmosphere_effect->GetCloudSvgfHistoryBoost();
						if (ImGui::SliderFloat("History Boost", &history_boost, 1.0f, 32.0f, "%.1f")) {
							atmosphere_effect->SetCloudSvgfHistoryBoost(history_boost);
						}

						float history_threshold = atmosphere_effect->GetCloudSvgfHistoryThreshold();
						if (ImGui::SliderFloat("History Threshold", &history_threshold, 1.0f, 64.0f, "%.1f")) {
							atmosphere_effect->SetCloudSvgfHistoryThreshold(history_threshold);
						}
					} else {
						ImGui::TextDisabled("Atmosphere effect not found.");
					}
				}

				// 3. Ambient & Individual Lights (from LightsWidget)
				if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
					AmbientSettingsComponent ambientSettings;
					ambientSettings.Draw(m_visualizer);

					ImGui::Separator();

					LightsComponent lights;
					lights.Draw(m_visualizer);
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
