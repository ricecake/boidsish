#include "ui/EffectsWidget.h"

#include "ConfigManager.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		EffectsWidget::EffectsWidget() {}

		void EffectsWidget::Draw() {
			if (!m_show) {
				return;
			}

			ImGui::Begin("Artistic Effects", &m_show);

			auto& config = ConfigManager::GetInstance();

			bool ripple_enabled = config.GetAppSettingBool("artistic_effect_ripple", false);
			if (ImGui::Checkbox("Ripple", &ripple_enabled)) {
				config.SetBool("artistic_effect_ripple", ripple_enabled);
			}

			bool color_shift_enabled = config.GetAppSettingBool("artistic_effect_color_shift", false);
			if (ImGui::Checkbox("Color Shift", &color_shift_enabled)) {
				config.SetBool("artistic_effect_color_shift", color_shift_enabled);
			}

			bool bnw_enabled = config.GetAppSettingBool("artistic_effect_black_and_white", false);
			if (ImGui::Checkbox("Black and White", &bnw_enabled)) {
				config.SetBool("artistic_effect_black_and_white", bnw_enabled);
			}

			bool negative_enabled = config.GetAppSettingBool("artistic_effect_negative", false);
			if (ImGui::Checkbox("Negative", &negative_enabled)) {
				config.SetBool("artistic_effect_negative", negative_enabled);
			}

			bool shimmery_enabled = config.GetAppSettingBool("artistic_effect_shimmery", false);
			if (ImGui::Checkbox("Shimmery", &shimmery_enabled)) {
				config.SetBool("artistic_effect_shimmery", shimmery_enabled);
			}

			bool glitched_enabled = config.GetAppSettingBool("artistic_effect_glitched", false);
			if (ImGui::Checkbox("Glitched", &glitched_enabled)) {
				config.SetBool("artistic_effect_glitched", glitched_enabled);
			}

			bool wireframe_enabled = config.GetAppSettingBool("artistic_effect_wireframe", false);
			if (ImGui::Checkbox("Wireframe", &wireframe_enabled)) {
				config.SetBool("artistic_effect_wireframe", wireframe_enabled);
			}

			bool curl_noise_enabled = config.GetAppSettingBool("artistic_effect_curl_noise", false);
			if (ImGui::Checkbox("Curl Noise", &curl_noise_enabled)) {
				config.SetBool("artistic_effect_curl_noise", curl_noise_enabled);
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
