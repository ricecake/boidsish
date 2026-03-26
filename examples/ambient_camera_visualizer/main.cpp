#include <iostream>
#include <memory>
#include <vector>

#include "ambient_camera.h"
#include "decor_manager.h"
#include "dot.h"
#include "graphics.h"
#include "light_manager.h"
#include "polyhedron.h"
#include "line.h"
#include "terrain_generator_interface.h"

using namespace Boidsish;

// Color constants
const glm::vec3 COLOR_PROBE(0.0f, 1.0f, 1.0f);   // Cyan
const glm::vec3 COLOR_CAMERA(1.0f, 0.0f, 1.0f);  // Magenta
const glm::vec3 COLOR_FOCUS(1.0f, 1.0f, 0.0f);   // Yellow

int main() {
    try {
        Visualizer visualizer(1280, 720, "Ambient Camera System Visualizer");

        // Setup Ambient Camera System
        AmbientCameraSystem ambient_system;

        // Ensure we have some terrain to interact with
        visualizer.AddPrepareCallback([](Visualizer& viz) {
            auto terrain = viz.GetTerrain();
            if (terrain) {
                terrain->SetWorldScale(5.0f);
            }
        });

        // Set camera to FREE mode to allow user inspection
        visualizer.SetCameraMode(CameraMode::FREE);
        Camera free_cam;
        free_cam.x = 0.0f;
        free_cam.y = 100.0f;
        free_cam.z = 200.0f;
        free_cam.pitch = -30.0f;
        visualizer.SetCamera(free_cam);

        // Dummy camera for ambient system update
        Camera dummy_cam;
        glm::vec3 probe_pos(0.0f);

        visualizer.AddShapeHandler([&](float time) {
            std::vector<std::shared_ptr<Shape>> shapes;

            float dt = visualizer.GetLastFrameTime();
            auto terrain = visualizer.GetTerrain();
            auto decor = visualizer.GetDecorManager();

            // Update ambient system
            ambient_system.Update(dt, terrain.get(), decor, dummy_cam, probe_pos);

            const auto& probeSpline = ambient_system.GetProbeSpline();
            const auto& cameraSpline = ambient_system.GetCameraSpline();
            const auto& focusSpline = ambient_system.GetFocusSpline();
            float globalU = ambient_system.GetGlobalU();

            auto addSplineVisuals = [&](const CoordinatedSpline& spline, const glm::vec3& color, int id_offset) {
                // Waypoints
                for (size_t i = 0; i < spline.waypoints.size(); ++i) {
                    const auto& wp = spline.waypoints[i];
                    auto dot = std::make_shared<Dot>(id_offset + i, wp.x, wp.y, wp.z, 2.0f);
                    dot->SetColor(color.r, color.g, color.b, 1.0f);
                    shapes.push_back(dot);
                }

                // Spline segments
                if (spline.totalLength > 0.0f) {
                    const int segments = 100;
                    Vector3 prev = spline.Evaluate(0.0f);
                    for (int i = 1; i <= segments; ++i) {
                        float u = (float)i / (float)segments;
                        Vector3 curr = spline.Evaluate(u);
                        auto line = std::make_shared<Line>(glm::vec3(prev.x, prev.y, prev.z), glm::vec3(curr.x, curr.y, curr.z), 0.5f);
                        line->SetColor(color.r, color.g, color.b, 0.5f);
                        shapes.push_back(line);
                        prev = curr;
                    }
                }
            };

            addSplineVisuals(probeSpline, COLOR_PROBE, 1000);
            addSplineVisuals(cameraSpline, COLOR_CAMERA, 2000);
            addSplineVisuals(focusSpline, COLOR_FOCUS, 3000);

            // Marker shapes for current positions
            Vector3 pPos = probeSpline.Evaluate(globalU);
            Vector3 cPos = cameraSpline.Evaluate(globalU);
            Vector3 fPos = focusSpline.Evaluate(globalU);

            auto pMarker = std::make_shared<Polyhedron>(PolyhedronType::Cube, 0, pPos.x, pPos.y, pPos.z, 4.0f);
            pMarker->SetColor(COLOR_PROBE.r, COLOR_PROBE.g, COLOR_PROBE.b, 1.0f);
            shapes.push_back(pMarker);

            auto cMarker = std::make_shared<Polyhedron>(PolyhedronType::Octahedron, 0, cPos.x, cPos.y, cPos.z, 3.0f);
            cMarker->SetColor(COLOR_CAMERA.r, COLOR_CAMERA.g, COLOR_CAMERA.b, 1.0f);
            shapes.push_back(cMarker);

            auto fMarker = std::make_shared<Polyhedron>(PolyhedronType::Icosahedron, 0, fPos.x, fPos.y, fPos.z, 2.0f);
            fMarker->SetColor(COLOR_FOCUS.r, COLOR_FOCUS.g, COLOR_FOCUS.b, 1.0f);
            shapes.push_back(fMarker);

            // Connection between camera and focus
            auto lookLine = std::make_shared<Line>(glm::vec3(cPos.x, cPos.y, cPos.z), glm::vec3(fPos.x, fPos.y, fPos.z), 0.3f);
            lookLine->SetColor(1.0f, 1.0f, 1.0f, 0.8f);
            lookLine->SetStyle(Line::Style::LASER);
            shapes.push_back(lookLine);

            return shapes;
        });

        visualizer.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
