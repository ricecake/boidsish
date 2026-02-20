#include "ui/ShadowsWidget.h"
#include "graphics.h"
#include "imgui.h"
#include "ConfigManager.h"

namespace Boidsish {
	namespace UI {

		ShadowsWidget::ShadowsWidget(Visualizer& visualizer): _visualizer(visualizer) {
			_show = true;
			auto& cfg = ConfigManager::GetInstance();
			_sdfSoftness = cfg.GetAppSettingFloat("sdf_shadow_softness", 10.0f);
			_sdfMaxDist = cfg.GetAppSettingFloat("sdf_shadow_max_dist", 2.0f);
			_sdfBias = cfg.GetAppSettingFloat("sdf_shadow_bias", 0.05f);
			_sdfDebug = cfg.GetAppSettingBool("sdf_shadow_debug", false);
		}

		void ShadowsWidget::Draw() {
			if (!_show)
				return;

			ImGui::Begin("Shadows", &_show);

			if (ImGui::CollapsingHeader("SDF Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& cfg = ConfigManager::GetInstance();
				bool  enabled = cfg.GetAppSettingBool("enable_sdf_shadows", true);
				if (ImGui::Checkbox("Enable SDF Shadows", &enabled)) {
					cfg.SetBool("enable_sdf_shadows", enabled);
				}

				bool changed = false;
				if (ImGui::SliderFloat("Softness", &_sdfSoftness, 0.1f, 100.0f))
					changed = true;
				if (ImGui::SliderFloat("Max Distance", &_sdfMaxDist, 0.1f, 10.0f))
					changed = true;
				if (ImGui::SliderFloat("Bias", &_sdfBias, 0.0f, 0.5f))
					changed = true;
				if (ImGui::Checkbox("Debug SDF", &_sdfDebug))
					changed = true;

				if (changed) {
					auto& cfg = ConfigManager::GetInstance();
					cfg.SetFloat("sdf_shadow_softness", _sdfSoftness);
					cfg.SetFloat("sdf_shadow_max_dist", _sdfMaxDist);
					cfg.SetFloat("sdf_shadow_bias", _sdfBias);
					cfg.SetBool("sdf_shadow_debug", _sdfDebug);
				}
			}

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Shadow Casters")) {
				// This would ideally list all shapes and their casts_shadows_ status
				// For now, let's just provide a global toggle or some examples
				ImGui::Text("Individual controls are set via API.");
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
