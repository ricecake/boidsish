#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

#include "graphics.h"
#include "light_manager.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"
#include "imgui.h"
#include "IWidget.h"
#include "terrain.h"
#include "dot.h"
#include "shape.h"
#include "vector.h"

using namespace Boidsish;

struct LightingSettings {
    glm::vec3 ambient_light = glm::vec3(0.05f, 0.05f, 0.07f);

    // Sun
    float sun_intensity = 1.0f;
    glm::vec3 sun_color = glm::vec3(1.0f, 0.95f, 0.8f);
    float sun_elevation = 45.0f; // degrees
    float sun_azimuth = 45.0f;   // degrees

    // Atmosphere
    float haze_density = 0.005f;
    float haze_height = 20.0f;
    glm::vec3 haze_color = glm::vec3(0.6f, 0.7f, 0.8f);

    float cloud_density = 0.5f;
    float cloud_altitude = 95.0f;
    float cloud_thickness = 10.0f;
    glm::vec3 cloud_color = glm::vec3(0.95f, 0.95f, 1.0f);

    // Bloom
    float bloom_intensity = 0.1f;
    float bloom_threshold = 1.0f;

    // Tone mapping
    int tone_mapping_mode = 1; // Filmic
};

class OptimizationManager {
public:
    OptimizationManager() : gen(rd()) {
        current_best = LightingSettings();
        GenerateCandidate();
    }

    void GenerateCandidate() {
        candidate = current_best;
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        float scale = step_size;

        // Perturb some parameters
        auto perturb = [&](float& val, float min_v, float max_v, float amount) {
            val += dis(gen) * amount * scale;
            if (val < min_v) val = min_v;
            if (val > max_v) val = max_v;
        };

        auto perturb_color = [&](glm::vec3& col, float amount) {
            perturb(col.r, 0.0f, 1.0f, amount);
            perturb(col.g, 0.0f, 1.0f, amount);
            perturb(col.b, 0.0f, 1.0f, amount);
        };

        perturb_color(candidate.ambient_light, 0.1f);
        perturb(candidate.sun_intensity, 0.0f, 10.0f, 0.5f);
        perturb_color(candidate.sun_color, 0.1f);
        perturb(candidate.sun_elevation, -90.0f, 90.0f, 10.0f);
        perturb(candidate.sun_azimuth, 0.0f, 360.0f, 20.0f);

        perturb(candidate.haze_density, 0.0f, 0.05f, 0.002f);
        perturb(candidate.haze_height, 0.0f, 100.0f, 5.0f);
        perturb_color(candidate.haze_color, 0.1f);

        perturb(candidate.cloud_density, 0.0f, 1.0f, 0.1f);
        perturb(candidate.cloud_altitude, 0.0f, 200.0f, 10.0f);
        perturb(candidate.cloud_thickness, 0.0f, 50.0f, 5.0f);
        perturb_color(candidate.cloud_color, 0.1f);

        perturb(candidate.bloom_intensity, 0.0f, 2.0f, 0.1f);
        perturb(candidate.bloom_threshold, 0.0f, 3.0f, 0.1f);

        if (std::uniform_real_distribution<float>(0, 1)(gen) < 0.2f * scale) {
            candidate.tone_mapping_mode = std::uniform_int_distribution<int>(0, 7)(gen);
        }
    }

    void SelectA() {
        // Current best is better
        step_size *= 0.95f;
        GenerateCandidate();
        showing_candidate = false;
    }

    void SelectB() {
        // Candidate is better
        current_best = candidate;
        step_size *= 1.05f;
        if (step_size > 2.0f) step_size = 2.0f;
        GenerateCandidate();
        showing_candidate = false; // Switch back to Option A (the new best)
    }

    void Apply(Visualizer& vis) {
        const LightingSettings& settings = showing_candidate ? candidate : current_best;

        auto& light_manager = vis.GetLightManager();
        light_manager.SetAmbientLight(settings.ambient_light);

        auto& lights = light_manager.GetLights();
        if (!lights.empty()) {
            auto& sun = lights[0];
            sun.intensity = settings.sun_intensity;
            sun.color = settings.sun_color;
            sun.type = DIRECTIONAL_LIGHT;

            float el = glm::radians(settings.sun_elevation);
            float az = glm::radians(settings.sun_azimuth);
            sun.direction = glm::normalize(glm::vec3(
                cos(el) * sin(az),
                -sin(el),
                cos(el) * cos(az)
            ));
            // Ensure position is far away in the opposite direction
            sun.position = -sun.direction * 1000.0f;
        }

        auto& pp_manager = vis.GetPostProcessingManager();
        for (auto& effect : pp_manager.GetPreToneMappingEffects()) {
            if (auto atmosphere = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect)) {
                atmosphere->SetHazeDensity(settings.haze_density);
                atmosphere->SetHazeHeight(settings.haze_height);
                atmosphere->SetHazeColor(settings.haze_color);
                atmosphere->SetCloudDensity(settings.cloud_density);
                atmosphere->SetCloudAltitude(settings.cloud_altitude);
                atmosphere->SetCloudThickness(settings.cloud_thickness);
                atmosphere->SetCloudColor(settings.cloud_color);
                atmosphere->SetEnabled(true);
            }
            if (auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect)) {
                bloom->SetIntensity(settings.bloom_intensity);
                bloom->SetThreshold(settings.bloom_threshold);
                bloom->SetEnabled(true);
            }
        }

        if (auto tone_mapping = std::dynamic_pointer_cast<PostProcessing::ToneMappingEffect>(pp_manager.GetToneMappingEffect())) {
            tone_mapping->SetMode((float)settings.tone_mapping_mode);
            tone_mapping->SetEnabled(true);
        }
    }

    std::string ExportSettings() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << "// Lighting Settings\n";
        ss << "Ambient: (" << current_best.ambient_light.r << ", " << current_best.ambient_light.g << ", " << current_best.ambient_light.b << ")\n";
        ss << "Sun Intensity: " << current_best.sun_intensity << "\n";
        ss << "Sun Color: (" << current_best.sun_color.r << ", " << current_best.sun_color.g << ", " << current_best.sun_color.b << ")\n";
        ss << "Sun Elevation: " << current_best.sun_elevation << "\n";
        ss << "Sun Azimuth: " << current_best.sun_azimuth << "\n";
        ss << "Haze Density: " << current_best.haze_density << "\n";
        ss << "Haze Height: " << current_best.haze_height << "\n";
        ss << "Haze Color: (" << current_best.haze_color.r << ", " << current_best.haze_color.g << ", " << current_best.haze_color.b << ")\n";
        ss << "Cloud Density: " << current_best.cloud_density << "\n";
        ss << "Cloud Altitude: " << current_best.cloud_altitude << "\n";
        ss << "Cloud Thickness: " << current_best.cloud_thickness << "\n";
        ss << "Cloud Color: (" << current_best.cloud_color.r << ", " << current_best.cloud_color.g << ", " << current_best.cloud_color.b << ")\n";
        ss << "Bloom Intensity: " << current_best.bloom_intensity << "\n";
        ss << "Bloom Threshold: " << current_best.bloom_threshold << "\n";
        ss << "Tone Mapping Mode: " << current_best.tone_mapping_mode << "\n";
        return ss.str();
    }

    LightingSettings current_best;
    LightingSettings candidate;
    bool showing_candidate = false;
    float step_size = 1.0f;

private:
    std::random_device rd;
    std::mt19937 gen;
};

class OptimizerWidget : public UI::IWidget {
public:
    OptimizerWidget(OptimizationManager& mgr, Visualizer& vis) : mgr_(mgr), vis_(vis) {}

    void Draw() override {
        ImGui::Begin("Lighting Optimizer");

        ImGui::Text("Current Step Size: %.3f", mgr_.step_size);

        if (ImGui::RadioButton("Option A (Current Best)", !mgr_.showing_candidate)) {
            mgr_.showing_candidate = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Option B (Candidate)", mgr_.showing_candidate)) {
            mgr_.showing_candidate = true;
        }

        if (ImGui::Button("A is better", ImVec2(120, 40))) {
            mgr_.SelectA();
        }
        ImGui::SameLine();
        if (ImGui::Button("B is better", ImVec2(120, 40))) {
            mgr_.SelectB();
        }

        ImGui::Separator();
        ImGui::Text("Current Best Settings:");
        ImGui::BeginChild("SettingsExport", ImVec2(0, 200), true);
        ImGui::TextUnformatted(mgr_.ExportSettings().c_str());
        ImGui::EndChild();

        if (ImGui::Button("Copy to Clipboard")) {
            ImGui::SetClipboardText(mgr_.ExportSettings().c_str());
        }

        ImGui::End();

        // Apply settings every frame to ensure the visualizer reflects current selection
        mgr_.Apply(vis_);
    }

private:
    OptimizationManager& mgr_;
    Visualizer& vis_;
};

int main() {
    try {
        Visualizer vis(1280, 720, "Lighting Optimizer");

        // Initialize the sun light explicitly
        Light sun = Light::CreateDirectional(
            glm::vec3(0, 100, 0),
            glm::vec3(0, -1, 0),
            1.0f,
            glm::vec3(1, 1, 1),
            true // Shadows
        );
        // Clear existing lights and add our sun
        vis.GetLightManager().GetLights().clear();
        vis.GetLightManager().AddLight(sun);

        OptimizationManager opt_mgr;
        auto widget = std::make_shared<OptimizerWidget>(opt_mgr, vis);
        vis.AddWidget(widget);

        vis.AddShapeHandler([](float time) {
            std::vector<std::shared_ptr<Shape>> shapes;

            // Central sphere to see lighting on smooth surface
            auto sphere = std::make_shared<Dot>(0, 0, 10, 0, 20, 2.0f, 1.0f, 1.0f);
            sphere->SetUsePBR(true);
            sphere->SetRoughness(0.3f);
            sphere->SetMetallic(0.8f);
            shapes.push_back(sphere);

            // Some other objects with different materials
            for (int i = 0; i < 5; ++i) {
                float angle = (float)i / 5.0f * 2.0f * 3.14159f;
                float x = cos(angle) * 10.0f;
                float z = sin(angle) * 10.0f;
                auto orb = std::make_shared<Dot>(i + 1, x, 5.0f, z, 10, 1.0f);
                orb->SetColor(0.8f, 0.2f, 0.2f);
                orb->SetUsePBR(true);
                orb->SetRoughness(0.1f + i * 0.2f);
                orb->SetMetallic(i % 2 == 0 ? 0.0f : 1.0f);
                shapes.push_back(orb);
            }

            return shapes;
        });

        // Setup camera
        Camera cam;
        cam.x = 0; cam.y = 15; cam.z = 40;
        cam.pitch = -20; cam.yaw = 0;
        vis.SetCamera(cam);

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
