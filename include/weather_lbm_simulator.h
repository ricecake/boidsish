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

        void Update(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain, const glm::vec3& cameraPos, float windSpeed, float windStrength, float targetTemp, float targetPressure, float targetHumidity);

        /**
         * @brief Updates the grid anchor based on camera position without stepping the simulation.
         */
        void UpdateAnchor(const glm::vec3& cameraPos, float totalTime, float timeOfDay);

        void PopulateWindData(WindDataUbo& ubo, std::vector<glm::vec4>& grid_out, float totalTime, float curlScale, float curlStrength) const;

        /**
         * @brief Takes a full snapshot of the simulation state for asynchronous readback.
         */
        void TakeSnapshot(LbmSnapshot& snapshot, float totalTime, float curlScale, float curlStrength) const;

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

        float GetTau() const { return tau_; }
        void  SetTau(float tau) {
            tau_ = std::max(0.51f, tau);
            omega_ = 1.0f / tau_;
        }

        void Reset(const ITerrainGenerator& terrain, float totalTime = 0.0f, float timeOfDay = 12.0f) {
            Initialize(terrain, totalTime, timeOfDay);
        }

        static constexpr int kPadding = 2;

        /**
         * @brief Manually inject a pressure burst or vacuum at a world position.
         * @param burstStrength If > 0, primes neighboring cells to move away from the center.
         */
        void InjectPressure(const glm::vec3& pos, float pressureHpa, float burstStrength);

        /**
         * @brief Manually inject aerosol concentration at a world position.
         */
        void InjectAerosol(const glm::vec3& pos, float concentration);

        /**
         * @brief Manually inject temperature at a world position.
         */
        void InjectTemperature(const glm::vec3& pos, float temperatureK);

    private:
        void Initialize(const ITerrainGenerator& terrain, float totalTime, float timeOfDay);
        void InitializeCell(int x, int z, float totalTime, float timeOfDay, LbmCell& cell);
        void Step(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain, float windSpeed, float windStrength);

        /**
         * @brief Calculates base temperature for a world position including diurnal, seasonal, and spatial gradients.
         */
        float GetBaseTemperature(float worldX, float worldZ, float totalTime, float timeOfDay) const;

        /**
         * @brief Returns a [0, 1] seasonal factor (0 = winter, 1 = summer).
         */
        float GetSeasonalFactor(float totalTime) const;

        void UpdateConfig(const ITerrainGenerator& terrain);
        void DeriveAtmosphere(float totalTime, float timeOfDay);

        // LBM Operators
        float CalculateEquilibrium(int i, float rho, glm::vec2 u);
        void CollisionAndStreaming();
        void ApplyPhysics(float deltaTime, float totalTime, float timeOfDay);
        void ApplyBoundaries(float totalTime, float windSpeed, float windStrength, float timeOfDay, float targetTemp, float targetPressure, float targetHumidity);
        void ShiftGrid(glm::ivec2 shiftOffset, float totalTime, float timeOfDay);

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
        static constexpr float dt_ = 0.1f; // Fixed simulation timestep
        float                  tau_ = 0.8f;
        float                  omega_ = 1.25f;

        // D2Q9 Constants
        static const int cx[9];
        static const int cz[9];
        static const float weights[9];
    };

} // namespace Boidsish
