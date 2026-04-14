#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "weather_lbm_types.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

    class WeatherLbmSimulator {
    public:
        WeatherLbmSimulator(int width, int height);
        ~WeatherLbmSimulator();

        void Update(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain);

        const PhysicallyBasedWeatherOutput& GetOutput() const { return currentOutput_; }

        // Debugging / Inspection
        const std::vector<LbmCell>& GetCells() const { return gridA_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }

    private:
        void Initialize(const ITerrainGenerator& terrain);
        void Step(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain);
        void UpdateConfig(const ITerrainGenerator& terrain);
        void DeriveAtmosphere(float timeOfDay);

        // LBM Operators
        float CalculateEquilibrium(int i, float rho, glm::vec2 u);
        void CollisionAndStreaming();
        void ApplyPhysics(float deltaTime, float totalTime, float timeOfDay);
        void ApplyBoundaries(float totalTime);

        int width_;
        int height_;
        std::vector<LbmCell> gridA_;
        std::vector<LbmCell> gridB_;
        std::vector<LbmCellConfig> config_;

        PhysicallyBasedWeatherOutput currentOutput_;

        // LBM Constants
        static constexpr float tau_ = 0.8f; // Relaxation time
        static constexpr float omega_ = 1.0f / tau_;

        // D2Q9 Constants
        static const int cx[9];
        static const int cz[9];
        static const float weights[9];
    };

} // namespace Boidsish
