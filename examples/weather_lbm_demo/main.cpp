#include <iostream>
#include <iomanip>
#include "weather_lbm.h"

int main() {
    Boidsish::WeatherLbmConfig config;
    config.width = 32;
    config.height = 32;
    config.buoyancy = 0.5f; // Increased for noticeable thermal induction
    config.thermal_rise = 0.05f;

    Boidsish::WeatherLbmSimulator simulator(config);

    std::cout << "Starting Improved Weather LBM Simulation Demo..." << std::endl;
    std::cout << "Grid size: " << config.width << "x" << config.height << std::endl;

    // Configure cells with varying properties
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            Boidsish::LbmCellConfig cell_cfg;

            // Middle region is very rough and hot
            if (x > 12 && x < 20 && y > 12 && y < 20) {
                cell_cfg.roughness = 0.8f;
                cell_cfg.sensible_heat_factor = 2.0f; // Rapidly heats

                // Hot spot in the middle
                Boidsish::LbmCell cell = simulator.GetCell(x, y);
                cell.temperature = 1.0f;
                simulator.SetCell(x, y, cell);
            }

            // Aerosol source
            if (y == 2 && x > 10 && x < 22) {
                cell_cfg.aerosol_release_rate = 5.0f;
            }

            simulator.SetCellConfig(x, y, cell_cfg);
        }
    }

    float totalTime = 0.0f;
    float dt = 0.05f;

    for (int step = 0; step < 200; ++step) {
        simulator.Update(dt, totalTime);
        totalTime += dt;

        // Print status
        if (step % 40 == 0) {
            std::cout << "\nStep " << step << " (Time: " << std::fixed << std::setprecision(2) << totalTime << "):" << std::endl;

            auto printCell = [&](int x, int y, const std::string& label) {
                float t = simulator.GetTemperature(x, y);
                float a = simulator.GetAerosol(x, y);
                glm::vec3 v = simulator.GetVelocity(x, y);
                float p = simulator.GetPressure(x, y);
                // v.y is UP in engine convention
                std::cout << "  " << std::left << std::setw(15) << label
                          << " T: " << std::fixed << std::setprecision(3) << t
                          << " A: " << std::setprecision(3) << a
                          << " P: " << std::setprecision(3) << p
                          << " V: (" << v.x << ", " << v.y << ", " << v.z << ") [y-up]" << std::endl;
            };

            printCell(16, 2, "Aerosol Source");
            printCell(16, 16, "Rough Hot Center");
            printCell(28, 28, "Open Corner");
        }
    }

    std::cout << "\nSimulation completed successfully." << std::endl;

    return 0;
}
