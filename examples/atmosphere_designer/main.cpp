#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ConfigManager.h"
#include "IWidget.h"
#include "graphics.h"
#include "imgui.h"
#include "logger.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"

using namespace Boidsish;

class AtmosphereDesignerWidget: public UI::IWidget {
public:
	AtmosphereDesignerWidget(Visualizer& vis): m_vis(vis) {}

	void Draw() override {
		if (ImGui::Begin("Atmosphere Designer")) {
			auto&                                             postManager = m_vis.GetPostProcessingManager();
			std::shared_ptr<PostProcessing::AtmosphereEffect> atmosphere;

			for (auto& effect : postManager.GetPreToneMappingEffects()) {
				if (effect->GetName() == "Atmosphere") {
					atmosphere = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
					break;
				}
			}

			if (atmosphere) {
				if (ImGui::CollapsingHeader("Atmosphere Scattering", ImGuiTreeNodeFlags_DefaultOpen)) {
					float rayleigh = atmosphere->GetRayleighScale();
					if (ImGui::SliderFloat("Rayleigh Scale", &rayleigh, 0.0f, 10.0f)) {
						atmosphere->SetRayleighScale(rayleigh);
					}
					float mie = atmosphere->GetMieScale();
					if (ImGui::SliderFloat("Mie Scale", &mie, 0.0f, 10.0f)) {
						atmosphere->SetMieScale(mie);
					}
					float mieAnisotropy = atmosphere->GetMieAnisotropy();
					if (ImGui::SliderFloat("Mie Anisotropy", &mieAnisotropy, 0.0f, 0.99f)) {
						atmosphere->SetMieAnisotropy(mieAnisotropy);
					}
					float multiScat = atmosphere->GetMultiScatScale();
					if (ImGui::SliderFloat("MultiScat Scale", &multiScat, 0.0f, 2.0f)) {
						atmosphere->SetMultiScatScale(multiScat);
					}
					float ambientScat = atmosphere->GetAmbientScatScale();
					if (ImGui::SliderFloat("Ambient Scat Scale", &ambientScat, 0.0f, 2.0f)) {
						atmosphere->SetAmbientScatScale(ambientScat);
					}
				}

				if (ImGui::CollapsingHeader("Haze", ImGuiTreeNodeFlags_DefaultOpen)) {
					float hazeDensity = atmosphere->GetHazeDensity();
					if (ImGui::SliderFloat("Haze Density", &hazeDensity, 0.0f, 0.05f, "%.4f")) {
						atmosphere->SetHazeDensity(hazeDensity);
					}
					float hazeHeight = atmosphere->GetHazeHeight();
					if (ImGui::SliderFloat("Haze Height", &hazeHeight, 0.0f, 200.0f)) {
						atmosphere->SetHazeHeight(hazeHeight);
					}
					glm::vec3 hazeColor = atmosphere->GetHazeColor();
					if (ImGui::ColorEdit3("Haze Color", &hazeColor[0])) {
						atmosphere->SetHazeColor(hazeColor);
					}
				}

				if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
					float cloudDensity = atmosphere->GetCloudDensity();
					if (ImGui::SliderFloat("Cloud Density", &cloudDensity, 0.0f, 2.0f)) {
						atmosphere->SetCloudDensity(cloudDensity);
					}
					float cloudAltitude = atmosphere->GetCloudAltitude();
					if (ImGui::SliderFloat("Cloud Altitude", &cloudAltitude, 0.0f, 1000.0f)) {
						atmosphere->SetCloudAltitude(cloudAltitude);
					}
					float cloudThickness = atmosphere->GetCloudThickness();
					if (ImGui::SliderFloat("Cloud Thickness", &cloudThickness, 0.0f, 200.0f)) {
						atmosphere->SetCloudThickness(cloudThickness);
					}
					glm::vec3 cloudColor = atmosphere->GetCloudColor();
					if (ImGui::ColorEdit3("Cloud Color", &cloudColor[0])) {
						atmosphere->SetCloudColor(cloudColor);
					}

					float cloudShadow = ConfigManager::GetInstance().GetAppSettingFloat("cloud_shadow_intensity", 0.5f);
					if (ImGui::SliderFloat("Cloud Shadow Intensity", &cloudShadow, 0.0f, 1.0f)) {
						ConfigManager::GetInstance().SetFloat("cloud_shadow_intensity", cloudShadow);
					}
				}

				if (ImGui::CollapsingHeader("Physical Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
					float atmosHeight = atmosphere->GetAtmosphereHeight();
					if (ImGui::SliderFloat("Atmosphere Height (km)", &atmosHeight, 10.0f, 100.0f)) {
						atmosphere->SetAtmosphereHeight(atmosHeight);
					}

					glm::vec3 rayleighScattering = atmosphere->GetRayleighScattering() * 1000.0f;
					if (ImGui::ColorEdit3("Rayleigh Scattering", &rayleighScattering[0])) {
						atmosphere->SetRayleighScattering(rayleighScattering * 0.001f);
					}

					float mieScat = atmosphere->GetMieScattering() * 1000.0f;
					if (ImGui::SliderFloat("Mie Scattering coeff", &mieScat, 0.0f, 10.0f)) {
						atmosphere->SetMieScattering(mieScat * 0.001f);
					}

					float mieExt = atmosphere->GetMieExtinction() * 1000.0f;
					if (ImGui::SliderFloat("Mie Extinction coeff", &mieExt, 0.0f, 10.0f)) {
						atmosphere->SetMieExtinction(mieExt * 0.001f);
					}

					glm::vec3 ozoneAbsorption = atmosphere->GetOzoneAbsorption() * 1000.0f;
					if (ImGui::ColorEdit3("Ozone Absorption", &ozoneAbsorption[0])) {
						atmosphere->SetOzoneAbsorption(ozoneAbsorption * 0.001f);
					}

					float rayleighH = atmosphere->GetRayleighScaleHeight();
					if (ImGui::SliderFloat("Rayleigh Scale Height (km)", &rayleighH, 1.0f, 20.0f)) {
						atmosphere->SetRayleighScaleHeight(rayleighH);
					}

					float mieH = atmosphere->GetMieScaleHeight();
					if (ImGui::SliderFloat("Mie Scale Height (km)", &mieH, 0.1f, 10.0f)) {
						atmosphere->SetMieScaleHeight(mieH);
					}
				}

				if (ImGui::CollapsingHeader("Atmosphere Variance", ImGuiTreeNodeFlags_DefaultOpen)) {
					float varScale = atmosphere->GetColorVarianceScale();
					if (ImGui::SliderFloat("Variance Scale", &varScale, 0.1f, 10.0f)) {
						atmosphere->SetColorVarianceScale(varScale);
					}

					float varStrength = atmosphere->GetColorVarianceStrength();
					if (ImGui::SliderFloat("Variance Strength", &varStrength, 0.0f, 0.5f)) {
						atmosphere->SetColorVarianceStrength(varStrength);
					}
				}
			} else {
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Atmosphere effect not found!");
			}

			if (ImGui::CollapsingHeader("Lighting & Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& lightManager = m_vis.GetLightManager();
				auto& lights = lightManager.GetLights();
				if (!lights.empty()) {
					auto& sun = lights[0];

					ImGui::Text("Sun Direction:");
					bool changed = false;
					if (ImGui::SliderFloat("Azimuth", &sun.azimuth, 0.0f, 360.0f))
						changed = true;
					if (ImGui::SliderFloat("Elevation", &sun.elevation, -90.0f, 90.0f))
						changed = true;

					if (changed) {
						sun.UpdateDirectionFromAngles();
					}

					ImGui::SliderFloat("Sun Intensity", &sun.intensity, 0.0f, 10.0f);
					ImGui::ColorEdit3("Sun Color", &sun.color[0]);
				}

				glm::vec3 ambient = lightManager.GetAmbientLight();
				if (ImGui::ColorEdit3("Ambient Light", &ambient[0])) {
					lightManager.SetAmbientLight(ambient);
				}
			}

			if (ImGui::CollapsingHeader("Day/Night Cycle", ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& cycle = m_vis.GetLightManager().GetDayNightCycle();
				ImGui::Checkbox("Cycle Enabled", &cycle.enabled);
				ImGui::SliderFloat("Time (0-24)", &cycle.time, 0.0f, 24.0f);
				ImGui::SliderFloat("Cycle Speed", &cycle.speed, 0.0f, 2.0f);
				ImGui::Checkbox("Cycle Paused", &cycle.paused);
			}

			ImGui::Separator();
			if (ImGui::Button("Save Settings", ImVec2(-FLT_MIN, 0))) {
				SaveSettings(atmosphere);
			}
		}
		ImGui::End();
	}

private:
	void SaveSettings(std::shared_ptr<PostProcessing::AtmosphereEffect> atmosphere) {
		std::ofstream file("atmosphere_settings.txt");
		if (!file.is_open()) {
			logger::ERROR("Failed to open atmosphere_settings.txt for writing");
			return;
		}

		file << "[Atmosphere]\n";
		if (atmosphere) {
			file << "RayleighScale=" << atmosphere->GetRayleighScale() << "\n";
			file << "MieScale=" << atmosphere->GetMieScale() << "\n";
			file << "MieAnisotropy=" << atmosphere->GetMieAnisotropy() << "\n";
			file << "MultiScatScale=" << atmosphere->GetMultiScatScale() << "\n";
			file << "AmbientScatScale=" << atmosphere->GetAmbientScatScale() << "\n";

			file << "HazeDensity=" << atmosphere->GetHazeDensity() << "\n";
			file << "HazeHeight=" << atmosphere->GetHazeHeight() << "\n";
			auto hc = atmosphere->GetHazeColor();
			file << "HazeColor=" << hc.r << "," << hc.g << "," << hc.b << "\n";

			file << "CloudDensity=" << atmosphere->GetCloudDensity() << "\n";
			file << "CloudAltitude=" << atmosphere->GetCloudAltitude() << "\n";
			file << "CloudThickness=" << atmosphere->GetCloudThickness() << "\n";
			auto cc = atmosphere->GetCloudColor();
			file << "CloudColor=" << cc.r << "," << cc.g << "," << cc.b << "\n";
			file << "CloudShadowIntensity="
				 << ConfigManager::GetInstance().GetAppSettingFloat("cloud_shadow_intensity", 0.5f) << "\n";

			file << "\n[PhysicalParameters]\n";
			file << "AtmosphereHeight=" << atmosphere->GetAtmosphereHeight() << "\n";
			auto rs = atmosphere->GetRayleighScattering();
			file << "RayleighScattering=" << rs.r << "," << rs.g << "," << rs.b << "\n";
			file << "MieScattering=" << atmosphere->GetMieScattering() << "\n";
			file << "MieExtinction=" << atmosphere->GetMieExtinction() << "\n";
			auto oa = atmosphere->GetOzoneAbsorption();
			file << "OzoneAbsorption=" << oa.r << "," << oa.g << "," << oa.b << "\n";
			file << "RayleighScaleHeight=" << atmosphere->GetRayleighScaleHeight() << "\n";
			file << "MieScaleHeight=" << atmosphere->GetMieScaleHeight() << "\n";

			file << "\n[AtmosphereVariance]\n";
			file << "ColorVarianceScale=" << atmosphere->GetColorVarianceScale() << "\n";
			file << "ColorVarianceStrength=" << atmosphere->GetColorVarianceStrength() << "\n";
		}

		auto& lightManager = m_vis.GetLightManager();
		auto& lights = lightManager.GetLights();
		if (!lights.empty()) {
			file << "\n[Lighting]\n";
			file << "SunIntensity=" << lights[0].intensity << "\n";
			auto sc = lights[0].color;
			file << "SunColor=" << sc.r << "," << sc.g << "," << sc.b << "\n";
			file << "SunAzimuth=" << lights[0].azimuth << "\n";
			file << "SunElevation=" << lights[0].elevation << "\n";
		}

		auto al = lightManager.GetAmbientLight();
		file << "AmbientLight=" << al.r << "," << al.g << "," << al.b << "\n";

		auto& cycle = lightManager.GetDayNightCycle();
		file << "\n[DayNightCycle]\n";
		file << "Enabled=" << (cycle.enabled ? "true" : "false") << "\n";
		file << "Time=" << cycle.time << "\n";
		file << "Speed=" << cycle.speed << "\n";

		file.close();
		logger::LOG("Atmosphere settings saved to atmosphere_settings.txt");
	}

	Visualizer& m_vis;
};

int main(int argc, char** argv) {
	Visualizer vis(1280, 720, "Atmosphere Designer");

	auto& config = ConfigManager::GetInstance();
	config.SetBool("render_terrain", false);
	config.SetBool("render_decor", false);
	config.SetBool("render_floor", false);
	config.SetBool("enable_shadows", false);
	config.SetFloat("ambient_particle_density", 0.0f);

	auto& lightManager = vis.GetLightManager();
	lightManager.GetDayNightCycle().paused = true;

	auto designer = std::make_shared<AtmosphereDesignerWidget>(vis);
	vis.AddWidget(designer);

	// Initial camera position looking slightly up
	Camera cam = vis.GetCamera();
	cam.x = 0;
	cam.y = 2;
	cam.z = 5;
	cam.pitch = 15;
	cam.yaw = 0;
	vis.SetCamera(cam);

	vis.Run();

	return 0;
}
