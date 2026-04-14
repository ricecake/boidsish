#include <iostream>
#include <iomanip>
#include "weather_lbm.h"

int main() {
    Boidsish::WeatherLbmConfig config;
    config.width = 32;
    config.height = 32;
    config.buoyancy = 0.05f;
    config.thermal_rise = 0.02f;

    Boidsish::WeatherLbmSimulator simulator(config);

    std::cout << "Starting Weather LBM Simulation Demo..." << std::endl;
    std::cout << "Grid size: " << config.width << "x" << config.height << std::endl;

    // Manually set an obstacle in the middle
    for (int i = 12; i < 20; ++i) {
        for (int j = 12; j < 20; ++j) {
            simulator.SetObstacle(i, j, true);
        }
    }

    // Manually set an aerosol source
    Boidsish::LbmCell sourceCell = simulator.GetCell(5, 5);
    sourceCell.aerosol = 10.0f;
    sourceCell.temperature = 1.0f; // Hot source
    simulator.SetCell(5, 5, sourceCell);

    float totalTime = 0.0f;
    float dt = 0.1f;

    for (int step = 0; step < 100; ++step) {
        simulator.Update(dt, totalTime);
        totalTime += dt;

        // Print status every 20 steps
        if (step % 20 == 0) {
            const Boidsish::LbmCell& c55 = simulator.GetCell(5, 5);
            const Boidsish::LbmCell& c1010 = simulator.GetCell(10, 10);
            const Boidsish::LbmCell& c2525 = simulator.GetCell(25, 25);

            std::cout << "Step " << step << " (Time: " << std::fixed << std::setprecision(2) << totalTime << "):" << std::endl;
            std::cout << "  Cell (5,5)   - Temp: " << c55.temperature << ", Aerosol: " << c55.aerosol << ", Vz: " << c55.vz << std::endl;
            std::cout << "  Cell (10,10) - Temp: " << c1010.temperature << ", Aerosol: " << c1010.aerosol << ", Vz: " << c1010.vz << std::endl;
            std::cout << "  Cell (25,25) - Temp: " << c2525.temperature << ", Aerosol: " << c2525.aerosol << ", Vz: " << c2525.vz << std::endl;
        }

        // Keep injecting aerosol at the source
        sourceCell = simulator.GetCell(5, 5);
        sourceCell.aerosol = 10.0f;
        simulator.SetCell(5, 5, sourceCell);
    }

    std::cout << "Simulation completed successfully." << std::endl;

    return 0;
}
