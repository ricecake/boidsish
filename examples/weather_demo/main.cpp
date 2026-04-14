#include "graphics.h"
#include "weather_manager.h"
#include "weather_simulation_manager.h"
#include "arrow.h"
#include "dot.h"
#include "logger.h"
#include "ConfigManager.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <GL/glew.h>

using namespace Boidsish;

struct LbmCell {
    float f[9];
    float temperature;
    float aerosol;
    float vVel;
};

class WeatherDemoHandler {
public:
     WeatherDemoHandler(Visualizer& vis) : vis_(vis) {
        // Add some initial aerosol sources
        // Positioned around the center of the 256x256 grid
        vis_.GetWeatherManager()->AddAerosolSource({ {128.0f, 128.0f}, 20.0f, 30.0f });
        vis_.GetWeatherManager()->AddAerosolSource({ {64.0f, 192.0f}, 15.0f, 20.0f });
    }

    std::vector<std::shared_ptr<Shape>> Update(float time) {
        std::vector<std::shared_ptr<Shape>> shapes;

        const auto* sim = vis_.GetWeatherManager()->GetSimulation();
        if (!sim) return shapes;

        int width = sim->GetWidth();
        int height = sim->GetHeight();
        float cellSize = sim->GetCellSize();

        // Downsample visualization for performance
        // We have 256x256 cells, step 16 gives 16x16 = 256 arrows
        int step = 16;

        // Read back SSBO for visualization (Note: Inefficient, for demo only)
        std::vector<LbmCell> cpuGrid(width * height);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->GetCurrentReadBuffer());
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, width * height * sizeof(LbmCell), cpuGrid.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        int shapeId = 10000;

        for (int y = step/2; y < height; y += step) {
            for (int x = step/2; x < width; x += step) {
                const auto& cell = cpuGrid[y * width + x];

                // Calculate macroscopic velocity
                float rho = 0.0f;
                glm::vec2 u(0.0f);
                static const glm::vec2 e[9] = {
                    glm::vec2(0,0), glm::vec2(1,0), glm::vec2(0,1), glm::vec2(-1,0), glm::vec2(0,-1),
                    glm::vec2(1,1), glm::vec2(-1,1), glm::vec2(-1,-1), glm::vec2(1,-1)
                };
                for (int i = 0; i < 9; ++i) {
                    rho += cell.f[i];
                    u += cell.f[i] * e[i];
                }
                if (rho > 0.001f) u /= rho;

                float worldX = x * cellSize;
                float worldZ = y * cellSize;
                float terrainH = std::get<0>(vis_.GetTerrainPropertiesAtPoint(worldX, worldZ));

                // Draw wind arrow if there is significant movement
                float speed = glm::length(glm::vec3(u.x, cell.vVel, u.y));
                if (speed > 0.05f) {
                    auto arrow = std::make_shared<Arrow>(
                        shapeId++, worldX, terrainH + 2.0f, worldZ
                    );
                    arrow->SetDirection(glm::vec3(u.x * 4.0f, cell.vVel * 4.0f, u.y * 4.0f));
                    // Color based on temperature or just wind
                    arrow->SetColor(0.4f, 0.7f, 1.0f, 0.8f);
                    shapes.push_back(arrow);
                }

                // Draw aerosol density
                if (cell.aerosol > 0.05f) {
                    auto dot = std::make_shared<Dot>(
                        worldX, terrainH + 5.0f + cell.vVel * 10.0f, worldZ,
                        cell.aerosol * 5.0f
                    );
                    // Smokey color
                    dot->SetColor(0.7f, 0.7f, 0.7f, std::min(cell.aerosol * 0.5f, 0.7f));
                    shapes.push_back(dot);
                }
            }
        }

        return shapes;
    }

private:
    Visualizer& vis_;
};

int main() {
    try {
        Visualizer vis(1280, 720, "Weather LBM Simulation Demo");

        // Position camera to see the grid (centered on 128, 128)
        vis.GetCamera().x = 128.0f;
        vis.GetCamera().y = 150.0f;
        vis.GetCamera().z = 400.0f;
        vis.GetCamera().yaw = 0.0f;
        vis.GetCamera().pitch = -30.0f;

        WeatherDemoHandler handler(vis);
        vis.AddShapeHandler([&handler](float time) {
            return handler.Update(time);
        });

        logger::LOG("=================================================");
        logger::LOG("Weather LBM Simulation Demo");
        logger::LOG(" - Blue arrows: Horizontal wind & vertical rising");
        logger::LOG(" - Grey dots: Aerosol concentration (smoke)");
        logger::LOG(" - Simulation reacts to terrain height");
        logger::LOG("=================================================");

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
