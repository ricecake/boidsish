#include "ui/PostProcessingWidget.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		PostProcessingWidget::PostProcessingWidget(PostProcessing::PostProcessingManager& manager): manager_(manager) {}

		void PostProcessingWidget::Draw() {
			ImGui::Begin("Post-Processing Effects");

			for (auto& effect : manager_.GetEffects()) {
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
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
