#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <cstdlib>
#include <ctime>

#include "ConfigManager.h"
#include "graphics.h"
#include "logger.h"
#include "light_manager.h"
#include "terrain_generator.h"
#include "terrain_generator_interface.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "decor_manager.h"
#include "procedural_generator.h"
#include "model.h"
#include "dot.h"
#include "light.h"
#include "fire_effect.h"
#include "spline.h"
#include "rigid_body.h"
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

            // Disable distracted elements
            ConfigManager::GetInstance().SetBool("enable_floor", false);
            ConfigManager::GetInstance().SetBool("enable_skybox", false);
            viz.TogglePostProcessingEffect("Atmosphere", false);

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

        // --- Probe & Path State ---
        auto probe = std::make_shared<Boidsish::Dot>(0, 0, 0, 0, 5.0f);
        probe->SetColor(0.2f, 0.8f, 1.0f, 0.5f);
        probe->SetScale(glm::vec3(5.0f));
        probe->SetRefractive(true, 1.5f);
        probe->SetTrailLength(0);

        struct PathState {
            std::vector<Vector3> waypoints;
            float progress = 0.0f; // 0 to waypoints.size() - 1
            float speed = 0.2f;    // progress units per second
            bool active = false;
        } path_state;

        auto pick_biome_target = [&](ITerrainGenerator& gen, const glm::vec3& current_pos) -> glm::vec3 {
            auto terrain_gen = dynamic_cast<TerrainGenerator*>(&gen);
            if (!terrain_gen) return current_pos + glm::vec3(100, 0, 100);

            for (int i = 0; i < 50; ++i) {
                float rx = current_pos.x + (std::rand() % 2000 - 1000);
                float rz = current_pos.z + (std::rand() % 2000 - 1000);
                float biome_val = terrain_gen->getBiomeControlValue(rx, rz);
                // Forest/Grass indices (roughly 0.125 to 0.5)
                if (biome_val >= 0.125f && biome_val < 0.5f) {
                    float h, _n;
                    std::tie(h, std::ignore) = gen.GetTerrainPropertiesAtPoint(rx, rz);
                    return glm::vec3(rx, h, rz);
                }
            }
            return current_pos + glm::vec3(200, 0, 0); // Fallback
        };

        auto generate_path = [&](ITerrainGenerator& gen, const glm::vec3& start, const glm::vec3& target) {
            path_state.waypoints.clear();
            path_state.waypoints.push_back(Vector3(start.x, start.y, start.z));

            int segments = 10;
            float world_scale = gen.GetWorldScale();
            float height_offset = 10.0f * world_scale;

            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                glm::vec3 p = glm::mix(start, target, t);

                // Sample terrain height at intermediate point
                auto [h, _] = gen.GetTerrainPropertiesAtPoint(p.x, p.z);
                p.y = h + height_offset;

                // LOS check (simple)
                glm::vec3 prev_p(path_state.waypoints.back().x, path_state.waypoints.back().y, path_state.waypoints.back().z);
                glm::vec3 dir = p - prev_p;
                float dist = glm::length(dir);
                if (dist > 0.001f) {
                    dir /= dist;
                    float hit_dist;
                    if (gen.Raycast(prev_p, dir, dist, hit_dist)) {
                        // Terrain blocking! Nudge upward.
                        p.y += height_offset * 0.5f;
                    }
                }

                path_state.waypoints.push_back(Vector3(p.x, p.y, p.z));
            }
            path_state.progress = 0.0f;
            path_state.active = true;
        };

        int probe_light_id = -1;
        float last_bubble_time = 0.0f;
        glm::vec3 probe_pos(0.0f, 50.0f, 0.0f);
        RigidBody cam_body(glm::vec3(0, 100, 0));
        cam_body.linear_friction_ = 5.0f;
        cam_body.angular_friction_ = 5.0f;

        float smoothed_probe_terrain_h = 0.0f;
        float smoothed_cam_terrain_h = 0.0f;
        bool first_frame = true;

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

            // --- Path & Probe Movement ---
            float dt = visualizer.GetLastFrameTime();
            auto terrain = visualizer.GetTerrain();

            if (terrain && (!path_state.active || path_state.progress >= (float)path_state.waypoints.size() - 2.1f)) {
                glm::vec3 target = pick_biome_target(*terrain, probe_pos);
                generate_path(*terrain, probe_pos, target);
            }

            if (path_state.active && path_state.waypoints.size() >= 4) {
                path_state.progress += path_state.speed * dt;

                // Spline sampling
                int i = (int)path_state.progress;
                float t = path_state.progress - i;

                // Clamp to valid range for CatmullRom (requires p0, p1, p2, p3)
                int i0 = std::max(0, i);
                int i1 = std::min((int)path_state.waypoints.size() - 1, i + 1);
                int i2 = std::min((int)path_state.waypoints.size() - 1, i + 2);
                int i3 = std::min((int)path_state.waypoints.size() - 1, i + 3);

                Vector3 p = Spline::CatmullRom(t, path_state.waypoints[i0], path_state.waypoints[i1], path_state.waypoints[i2], path_state.waypoints[i3]);
                probe_pos = glm::vec3(p.x, p.y, p.z);
            }

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
            float cam_orbit_speed = 0.2f;
            float cam_dist = 100.0f;
            float cam_height_offset = 40.0f;

            // Calculate desired camera position orbiting the probe
            glm::vec3 desired_cam_pos;
            desired_cam_pos.x = probe_pos.x + cam_dist * std::cos(time * cam_orbit_speed);
            desired_cam_pos.z = probe_pos.z + cam_dist * std::sin(time * cam_orbit_speed);

            auto [h_cam_terrain, _c] = visualizer.GetTerrainPropertiesAtPoint(desired_cam_pos.x, desired_cam_pos.z);
            desired_cam_pos.y = std::max(h_cam_terrain + 10.0f, probe_pos.y + cam_height_offset);

            // Smoothing via RigidBody
            glm::vec3 cam_pos = cam_body.GetPosition();
            if (first_frame) {
                cam_pos = desired_cam_pos;
                cam_body.SetPosition(cam_pos);
            }

            // Simple spring-damper to target pos
            glm::vec3 to_desired = desired_cam_pos - cam_pos;
            cam_body.AddForce(to_desired * 50.0f);
            cam_body.Update(dt);

            // Orientation smoothing
            glm::vec3 front = glm::normalize(probe_pos - cam_pos);
            glm::quat target_rot = glm::quatLookAt(front, glm::vec3(0, 1, 0));

            glm::quat current_rot = cam_body.GetOrientation();
            glm::quat final_rot = glm::slerp(current_rot, target_rot, std::min(1.0f, dt * 2.0f));
            cam_body.SetOrientation(final_rot);

            // Apply final state to visualizer camera
            auto& cam = visualizer.GetCamera();
            glm::vec3 final_cam_pos = cam_body.GetPosition();
            cam.x = final_cam_pos.x;
            cam.y = final_cam_pos.y;
            cam.z = final_cam_pos.z;

            glm::vec3 cam_front = final_rot * glm::vec3(0, 0, -1);
            cam.yaw = glm::degrees(atan2(cam_front.x, -cam_front.z));
            cam.pitch = glm::degrees(asin(cam_front.y));

            first_frame = false;

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
