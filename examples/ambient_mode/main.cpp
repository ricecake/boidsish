#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <cstdlib>
#include <ctime>

#include "graphics.h"
#include "logger.h"
#include "light_manager.h"
#include "terrain_generator_interface.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "decor_manager.h"
#include "procedural_generator.h"
#include "model.h"
#include "dot.h"
#include "light.h"
#include "fire_effect.h"
#include "post_processing/PostProcessingManager.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Ambient Mode: Alaska Sunset");
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        // --- State variables ---
        bool sun_on = true;

        // Prepare will be called by Run, but we can add callbacks
        visualizer.AddPrepareCallback([](Visualizer& viz) {
            logger::LOG("Ambient Mode preparing...");

            // Initial world scale
            auto terrain = viz.GetTerrain();
            if (terrain) {
                terrain->SetWorldScale(5.0f);
            }

            // Alaska-style sunset setup
            auto& light_manager = viz.GetLightManager();
            auto& cycle = light_manager.GetDayNightCycle();
            cycle.enabled = false; // We will manually drive the sun

            // Initial sun position: at the horizon
            auto& lights = light_manager.GetLights();
            if (!lights.empty() && lights[0].type == DIRECTIONAL_LIGHT) {
                lights[0].azimuth = 180.0f; // South/West
                lights[0].elevation = -1.0f; // Just below the horizon for sunset colors
                lights[0].UpdateDirectionFromAngles();
                lights[0].casts_shadow = false; // Night is dominant, disable shadows
            }

            // Decor: Small buildings in the trees
            auto decor_manager = viz.GetDecorManager();
            if (decor_manager) {
                DecorProperties props;
                props.SetDensity(0.005f); // Sparse
                props.base_scale = 1.0f;
                props.scale_variance = 0.5f;
                props.align_to_terrain = true;
                props.random_yaw = true;

                // Restrict to Grassland/Forest biomes (first 8 biomes are Grassland sub-biomes)
                for (int i = 0; i < 8; ++i) {
                    props.biomes.set(static_cast<Biome>(i));
                }

                decor_manager->AddProceduralDecor(ProceduralType::Structure, props, 5);
            }
        });

        // Probe logic variables
        auto probe = std::make_shared<Boidsish::Dot>(0, 0, 0, 0, 1.0f);
        probe->SetColor(0.2f, 0.8f, 1.0f);
        probe->SetScale(glm::vec3(1.0f));
        int probe_light_id = -1;
        float last_bubble_time = 0.0f;
        glm::vec3 probe_pos(100.0f, 0.0f, 100.0f);
        glm::vec3 probe_velocity(0.0f);

        // Driving sun orbit and night factor override, and Probe logic
        visualizer.AddShapeHandler([&](float time) {
            auto& light_manager = visualizer.GetLightManager();
            auto& cycle = light_manager.GetDayNightCycle();
            cycle.night_factor = 1.0f; // Maximum fireflies

            auto& lights = light_manager.GetLights();
            if (!lights.empty() && lights[0].type == DIRECTIONAL_LIGHT) {
                // Sun slowly orbits the horizon
                float sun_orbit_speed = 0.05f;
                lights[0].azimuth = 180.0f + 90.0f * std::sin(time * sun_orbit_speed);
                lights[0].elevation = -0.5f + 1.5f * std::cos(time * sun_orbit_speed * 0.7f);
                lights[0].UpdateDirectionFromAngles();

                lights[0].base_intensity = sun_on ? 1.0f : 0.0f;
            }

            // --- Probe Movement ---
            float dt = visualizer.GetLastFrameTime();

            // Search for low points (simple random walk biased towards valleys)
            glm::vec3 random_accel(
                (std::rand() % 100 - 50) * 0.1f,
                0.0f,
                (std::rand() % 100 - 50) * 0.1f
            );

            // Sample neighbors to find valley direction
            glm::vec3 valley_bias(0.0f);
            auto [current_h, _] = visualizer.GetTerrainPropertiesAtPoint(probe_pos.x, probe_pos.z);
            auto [h_px, _1] = visualizer.GetTerrainPropertiesAtPoint(probe_pos.x + 5.0f, probe_pos.z);
            auto [h_nx, _2] = visualizer.GetTerrainPropertiesAtPoint(probe_pos.x - 5.0f, probe_pos.z);
            auto [h_pz, _3] = visualizer.GetTerrainPropertiesAtPoint(probe_pos.x, probe_pos.z + 5.0f);
            auto [h_nz, _4] = visualizer.GetTerrainPropertiesAtPoint(probe_pos.x, probe_pos.z - 5.0f);

            valley_bias.x = h_nx - h_px;
            valley_bias.z = h_nz - h_pz;

            probe_velocity += (random_accel + valley_bias * 2.0f) * dt;
            float max_speed = 5.0f;
            if (glm::length(probe_velocity) > max_speed) {
                probe_velocity = glm::normalize(probe_velocity) * max_speed;
            }
            probe_pos += probe_velocity * dt;
            probe_pos.y = current_h + 2.0f + 0.5f * std::sin(time * 2.0f); // Float above ground

            probe->SetPosition(probe_pos.x, probe_pos.y, probe_pos.z);

            // --- Probe Light ---
            if (probe_light_id == -1) {
                probe_light_id = light_manager.AddLight(Light::CreateEmissive(probe_pos, 5.0f, {0.2f, 0.8f, 1.0f}, 1.0f));
            } else {
                auto light = light_manager.GetLight(probe_light_id);
                if (light) {
                    light->position = probe_pos;
                    light->intensity = 5.0f + 2.0f * std::sin(time * 5.0f); // Pulsing glow
                }
            }

            // --- Bubble Effect ---
            if (time - last_bubble_time > 5.0f) {
                visualizer.AddFireEffect(probe_pos, FireEffectStyle::Bubbles, {0,1,0}, {0,2,0}, 20, 3.0f);
                last_bubble_time = time;
            }

            // --- Cinematic Camera ---
            auto& cam = visualizer.GetCamera();
            float cam_speed = 0.03f;
            float cam_dist = 200.0f;
            float cam_height_base = 60.0f;

            // Orbit the probe area
            cam.x = probe_pos.x + cam_dist * std::cos(time * cam_speed);
            cam.z = probe_pos.z + cam_dist * std::sin(time * cam_speed);

            // Oscillate height
            float h_viz = cam_height_base + 30.0f * std::sin(time * cam_speed * 1.5f);
            auto [h_terrain, std_ignore] = visualizer.GetTerrainPropertiesAtPoint(cam.x, cam.z);
            cam.y = std::max(h_terrain + 10.0f, h_viz);

            // Focus point: halfway between probe and center of orbit
            glm::vec3 target = (probe_pos + glm::vec3(cam.x, cam.y, cam.z)) * 0.5f;
            visualizer.LookAt(target);

            return std::vector<std::shared_ptr<Boidsish::Shape>>{probe};
        });

        // --- Input Controls ---
        visualizer.AddInputCallback([&](const Boidsish::InputState& state) {
            auto& post_manager = visualizer.GetPostProcessingManager();
            std::shared_ptr<PostProcessing::AutoExposureEffect> exposure;

            for (auto& effect : post_manager.GetPreToneMappingEffects()) {
                if (auto ae = std::dynamic_pointer_cast<PostProcessing::AutoExposureEffect>(effect)) {
                    exposure = ae;
                    break;
                }
            }

            if (exposure) {
                float lum = exposure->GetTargetLuminance();
                if (state.keys[GLFW_KEY_UP]) lum += 0.1f * state.delta_time;
                if (state.keys[GLFW_KEY_DOWN]) lum -= 0.1f * state.delta_time;
                exposure->SetTargetLuminance(std::clamp(lum, 0.05f, 2.0f));
            }

            if (state.key_down[GLFW_KEY_SPACE]) {
                sun_on = !sun_on;
            }
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
