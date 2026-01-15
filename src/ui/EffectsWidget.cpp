#include "ui/EffectsWidget.h"
#include "graphics.h"
#include "imgui.h"

namespace Boidsish {
    namespace UI {

        EffectsWidget::EffectsWidget(Visualizer& visualizer)
            : m_visualizer(visualizer) {}

        void EffectsWidget::Draw() {
            if (!m_show) {
                return;
            }

            ImGui::Begin("Artistic Effects", &m_show);

            bool ripple_enabled = m_visualizer.IsRippleEffectEnabled();
            if (ImGui::Checkbox("Ripple", &ripple_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::RIPPLE);
            }

            bool color_shift_enabled = m_visualizer.IsColorShiftEffectEnabled();
            if (ImGui::Checkbox("Color Shift", &color_shift_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::COLOR_SHIFT);
            }

            bool bnw_enabled = m_visualizer.IsBlackAndWhiteEffectEnabled();
            if (ImGui::Checkbox("Black and White", &bnw_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::BLACK_AND_WHITE);
            }

            bool negative_enabled = m_visualizer.IsNegativeEffectEnabled();
            if (ImGui::Checkbox("Negative", &negative_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::NEGATIVE);
            }

            bool shimmery_enabled = m_visualizer.IsShimmeryEffectEnabled();
            if (ImGui::Checkbox("Shimmery", &shimmery_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::SHIMMERY);
            }

            bool glitched_enabled = m_visualizer.IsGlitchedEffectEnabled();
            if (ImGui::Checkbox("Glitched", &glitched_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::GLITCHED);
            }

            bool wireframe_enabled = m_visualizer.IsWireframeEffectEnabled();
            if (ImGui::Checkbox("Wireframe", &wireframe_enabled)) {
                m_visualizer.ToggleEffect(VisualEffect::WIREFRAME);
            }

            ImGui::End();
        }

    } // namespace UI
} // namespace Boidsish
