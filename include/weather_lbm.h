#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace Boidsish {

    /**
     * @brief LbmCell represents a single cell in the Lattice Boltzmann simulation.
     * Layout is exactly 48 bytes:
     * - 9 floats for D2Q9 distributions (0-8)
     * - 1 float for temperature
     * - 1 float for aerosol concentration
     * - 1 float for vertical velocity
     */
    struct LbmCell {
        float f[9];        // 36 bytes: D2Q9 distributions
        float temperature; // 4 bytes
        float aerosol;     // 4 bytes
        float vz;          // 4 bytes: Vertical velocity (2.5D LBM)
    };

    static_assert(sizeof(LbmCell) == 48, "LbmCell must be exactly 48 bytes");

    /**
     * @brief Per-cell configuration for environmental properties.
     */
    struct LbmCellConfig {
        float roughness = 0.01f;          // Per-cell drag factor
        float aerosol_release_rate = 0.0f; // Continuous aerosol source
        float sensible_heat_factor = 1.0f; // Rate of heat gain/loss
    };

    struct WeatherLbmConfig {
        int width = 128;
        int height = 128;
        float omega = 1.0f;       // Relaxation parameter (1/tau)
        float buoyancy = 0.01f;   // Boussinesq buoyancy coefficient
        float drag = 0.01f;       // Local drag coefficient
        float thermal_rise = 0.01f; // Thermal rising coefficient
        float aerosol_decay = 0.001f;
        float temp_decay = 0.001f;
        float spatial_scale = 0.1f;
        float time_scale = 0.01f;
    };

    class WeatherLbmSimulator {
    public:
        WeatherLbmSimulator(const WeatherLbmConfig& config);
        ~WeatherLbmSimulator();

        /**
         * @brief Advance the simulation by one step.
         * @param deltaTime The time step.
         * @param totalTime Total elapsed time for noise-based boundaries and cycles.
         */
        void Update(float deltaTime, float totalTime);

        /**
         * @brief Manually set properties for a grid cell.
         */
        void SetCell(int x, int y, const LbmCell& cell);

        /**
         * @brief Get properties for a grid cell.
         */
        const LbmCell& GetCell(int x, int y) const;

        /**
         * @brief Set if a cell is an obstacle (non-rectangular grid support).
         */
        void SetObstacle(int x, int y, bool is_obstacle);

        /**
         * @brief Set per-cell configuration.
         */
        void SetCellConfig(int x, int y, const LbmCellConfig& config);

        /**
         * @brief Get macro-scale weather data for a cell.
         */
        float GetTemperature(int x, int y) const;
        float GetAerosol(int x, int y) const;
        glm::vec3 GetVelocity(int x, int y) const;
        float GetPressure(int x, int y) const;

        /**
         * @brief Get the grid dimensions.
         */
        int GetWidth() const { return config_.width; }
        int GetHeight() const { return config_.height; }

    private:
        void CollideAndStream(float dt, float totalTime);
        void ApplyBoundaries(float totalTime);
        void ComputeMacros(int x, int y, float& rho, glm::vec2& u) const;

        WeatherLbmConfig config_;
        std::vector<LbmCell> cells_;
        std::vector<LbmCell> next_cells_;
        std::vector<bool> obstacles_;
        std::vector<LbmCellConfig> cell_configs_;

        // D2Q9 constants
        static constexpr int dx[9] = { 0, 1, 0, -1, 0, 1, -1, -1, 1 };
        static constexpr int dy[9] = { 0, 0, 1, 0, -1, 1, 1, -1, -1 };
        static constexpr float weights[9] = {
            4.0f / 9.0f,
            1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
            1.0f / 36.0f, 1.0f / 36.0f, 1.0f / 36.0f, 1.0f / 36.0f
        };
    };

} // namespace Boidsish
