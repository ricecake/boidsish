#include "ui/LightningWidget.h"
#include "graphics.h"
#include "imgui.h"
#include "lightning_manager.h"
#include "weather_manager.h"
#include "terrain_generator_interface.h"
#include <algorithm>

namespace Boidsish {
	namespace UI {

		LightningWidget::LightningWidget(Visualizer& visualizer) : m_visualizer(visualizer) {}

		void LightningWidget::Draw() {
			if (!m_show) return;

			auto lightning = m_visualizer.GetLightningManager();
			if (!lightning) return;

			// Handle delayed strikes
			float dt = ImGui::GetIO().DeltaTime;
			for (auto it = m_delayedStrikes.begin(); it != m_delayedStrikes.end(); ) {
				it->timeLeft -= dt;
				if (it->timeLeft <= 0.0f) {
					TriggerManualStrike(it->type, it->color);
					it = m_delayedStrikes.erase(it);
				} else {
					++it;
				}
			}

			// Handle manual auto-trigger
			if (m_manualAutoTrigger) {
				float freq = lightning->GetFrequencyMultiplier();
				float chance = 0.05f * dt * freq;
				if ((static_cast<float>(rand()) / RAND_MAX) < chance) {
					int type = rand() % 3;
					TriggerManualStrike(type, glm::vec3(0.9f, 0.9f, 1.0f));
				}
			}

			ImGui::SetNextWindowPos(ImVec2(530, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(400, 450), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Lightning Control", &m_show)) {
				if (ImGui::CollapsingHeader("Global Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
					float intensity = lightning->GetIntensityMultiplier();
					if (ImGui::SliderFloat("Intensity Multiplier", &intensity, 0.0f, 10.0f, "%.2f")) {
						lightning->SetIntensityMultiplier(intensity);
					}

					float frequency = lightning->GetFrequencyMultiplier();
					if (ImGui::SliderFloat("Frequency Multiplier", &frequency, 0.0f, 50.0f, "%.2f")) {
						lightning->SetFrequencyMultiplier(frequency);
					}

					float lifetime = lightning->GetLifetimeMultiplier();
					if (ImGui::SliderFloat("Lifetime Multiplier", &lifetime, 0.1f, 5.0f, "%.2f")) {
						lightning->SetLifetimeMultiplier(lifetime);
					}

					float branchProb = lightning->GetBranchProbability();
					if (ImGui::SliderFloat("Branch Probability", &branchProb, 0.0f, 1.0f, "%.2f")) {
						lightning->SetBranchProbability(branchProb);
					}

					ImGui::Checkbox("Manual Auto-trigger", &m_manualAutoTrigger);
					ImGui::SetItemTooltip("Spawns random lightning near camera regardless of weather.");
				}

				if (ImGui::CollapsingHeader("Manual Trigger", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit3("Strike Color", &m_manualColor[0]);
					ImGui::SliderFloat("Strike Delay (s)", &m_strikeDelay, 0.0f, 10.0f, "%.2f");

					if (ImGui::Button("Trigger BOLT")) {
						if (m_strikeDelay > 0.0f) m_delayedStrikes.push_back({m_strikeDelay, 0, m_manualColor});
						else TriggerManualStrike(0, m_manualColor);
					}
					ImGui::SameLine();
					if (ImGui::Button("Trigger FORK")) {
						if (m_strikeDelay > 0.0f) m_delayedStrikes.push_back({m_strikeDelay, 1, m_manualColor});
						else TriggerManualStrike(1, m_manualColor);
					}
					ImGui::SameLine();
					if (ImGui::Button("Trigger CLOUD_TO_CLOUD")) {
						if (m_strikeDelay > 0.0f) m_delayedStrikes.push_back({m_strikeDelay, 2, m_manualColor});
						else TriggerManualStrike(2, m_manualColor);
					}

					if (!m_delayedStrikes.empty()) {
						ImGui::Text("Pending Delayed Strikes: %zu", m_delayedStrikes.size());
						if (ImGui::Button("Clear Pending")) m_delayedStrikes.clear();
					}
				}

				if (ImGui::CollapsingHeader("Active Strikes", ImGuiTreeNodeFlags_DefaultOpen)) {
					const auto& strikes = lightning->GetActiveStrikes();
					ImGui::Text("Currently Rendering: %zu", strikes.size());
					for (const auto& s : strikes) {
						ImGui::Text(" ID: %d | Type: %d | Intensity: %.2f", s.id, (int)s.type, s.intensity);
					}
				}
			}
			ImGui::End();
		}

		void LightningWidget::TriggerManualStrike(int typeInt, const glm::vec3& color) {
			auto lightning = m_visualizer.GetLightningManager();
			if (!lightning) return;

			LightningType type = static_cast<LightningType>(typeInt);
			glm::vec3 startPos = GetRandomPositionNearCamera();
			glm::vec3 endPos = startPos;

			if (type == LightningType::CLOUD_TO_CLOUD) {
				endPos.x += (rand() % 400 - 200);
				endPos.z += (rand() % 400 - 200);
				endPos.y += (rand() % 100 - 50);
			} else {
				endPos.y = 0.0f;
				auto terrain = m_visualizer.GetTerrain();
				if (terrain) {
					auto [h, n] = terrain->GetTerrainPropertiesAtPoint(endPos.x, endPos.z);
					endPos.y = h;
				}
			}

			lightning->TriggerStrike(type, startPos, endPos, color);
		}

		glm::vec3 LightningWidget::GetRandomPositionNearCamera() {
			const auto& camera = m_visualizer.GetCamera();
			auto weather = m_visualizer.GetWeatherManager();
			auto terrain = m_visualizer.GetTerrain();

			float cloudAltitude = 500.0f;
			if (weather) {
				cloudAltitude = weather->GetCurrentWeather().cloud_altitude;
			}

			float worldScaleVal = terrain ? terrain->GetWorldScale() : 1.0f;

			return glm::vec3(
				camera.x + (rand() % 1000 - 500),
				cloudAltitude * worldScaleVal,
				camera.z + (rand() % 1000 - 500)
			);
		}

	} // namespace UI
} // namespace Boidsish
