#include "ui/AudioWidget.h"
#include "graphics.h"
#include "audio_manager.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		AudioWidget::AudioWidget(Visualizer& visualizer) : m_visualizer(visualizer) {}

		void AudioWidget::Draw() {
			if (!m_visible) return;

			ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Audio Controls", &m_visible)) {
				auto& audio = m_visualizer.GetAudioManager();

				float master = audio.GetMasterVolume();
				if (ImGui::SliderFloat("Master Volume", &master, 0.0f, 1.0f)) {
					audio.SetMasterVolume(master);
				}

				float music = audio.GetMusicVolume();
				if (ImGui::SliderFloat("Music Volume", &music, 0.0f, 1.0f)) {
					audio.SetMusicVolume(music);
				}

				float sfx = audio.GetSfxVolume();
				if (ImGui::SliderFloat("SFX Volume", &sfx, 0.0f, 1.0f)) {
					audio.SetSfxVolume(sfx);
				}

				ImGui::Separator();
				ImGui::Text("Audio Statistics");

				ImGui::Separator();
				if (ImGui::Button("Stop All Sounds")) {
					audio.StopAllSounds();
				}
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
