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

        void Update(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain, const glm::vec3& cameraPos);

        const PhysicallyBasedWeatherOutput& GetOutput() const { return currentOutput_; }

        /**
         * @brief Get simulation state at a specific world position.
         */
        const LbmCell* GetCellAtPosition(const glm::vec3& pos) const;

        /**
         * @brief Get derived atmospheric state for a cell at a specific world position.
         * (Currently returns global state, but can be localized in the future).
         */
        PhysicallyBasedWeatherOutput GetWeatherAtPosition(const glm::vec3& pos) const;

        // Debugging / Inspection
        const std::vector<LbmCell>& GetCells() const { return *currentGrid_; }
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
        std::vector<LbmCell> grid1_;
        std::vector<LbmCell> grid2_;
        std::vector<LbmCell>* currentGrid_;
        std::vector<LbmCell>* nextGrid_;
        std::vector<LbmCellConfig> config_;

        PhysicallyBasedWeatherOutput currentOutput_;
        bool initialized_ = false;
        float accumulator_ = 0.0f;
        glm::ivec2 gridAnchor_ = {0, 0}; // Grid origin in chunk coordinates

        // LBM Constants
        static constexpr float dt_ = 0.1f;  // Fixed simulation timestep
        static constexpr float tau_ = 0.8f; // Relaxation time
        static constexpr float omega_ = 1.0f / tau_;

        // D2Q9 Constants
        static const int cx[9];
        static const int cz[9];
        static const float weights[9];
    };

} // namespace Boidsish
