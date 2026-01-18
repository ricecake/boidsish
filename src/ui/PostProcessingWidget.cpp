#include "ui/PostProcessingWidget.h"

#include "imgui.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"

namespace Boidsish {
	namespace UI {

		PostProcessingWidget::PostProcessingWidget(PostProcessing::PostProcessingManager& manager): manager_(manager) {}

		void PostProcessingWidget::Draw() {
			ImGui::Begin("Post-Processing Effects");

			for (auto& effect : manager_.GetPreToneMappingEffects()) {
				bool is_enabled = effect->IsEnabled();
				if (ImGui::Checkbox(effect->GetName().c_str(), &is_enabled)) {
					effect->SetEnabled(is_enabled);
				}

				if (effect->GetName() == "Film Grain" && is_enabled) {
					auto film_grain_effect = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
					if (film_grain_effect) {
						float intensity = film_grain_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f)) {
							film_grain_effect->SetIntensity(intensity);
						}
					}
				}

				if (effect->GetName() == "Bloom" && is_enabled) {
					auto bloom_effect = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect);
					if (bloom_effect) {
						float intensity = bloom_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity##Bloom", &intensity, 0.0f, 2.0f)) {
							bloom_effect->SetIntensity(intensity);
						}
						float threshold = bloom_effect->GetThreshold();
						if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 5.0f)) {
							bloom_effect->SetThreshold(threshold);
						}
					}
				}
			}

			if (auto tone_mapping_effect = manager_.GetToneMappingEffect()) {
				ImGui::Separator();
				bool is_enabled = tone_mapping_effect->IsEnabled();
				// The tone mapping effect should not be disable-able from the UI
				// if (ImGui::Checkbox(tone_mapping_effect->GetName().c_str(), &is_enabled)) {
				// 	tone_mapping_effect->SetEnabled(is_enabled);
				// }
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
