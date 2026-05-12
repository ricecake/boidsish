#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Boidsish {

    /**
     * @brief D2Q9 Lattice Boltzmann Method cell state.
     * Total size: 48 bytes (12 floats).
     *
     * f[0-8]: Particle distribution functions for D2Q9 velocities.
     * temperature: Local air temperature (Celsius or Kelvin, internally normalized).
     * aerosol: Concentration of aerosols (dust/pollen/etc).
     * vy: Vertical velocity component (2.5D simulation).
     */
    struct LbmCell {
        float f[9];      // D2Q9 distributions
        float temperature;
        glm::vec4 aerosols; // 4 distinct aerosol types (e.g. Dust, Pollen, Smoke, Mist)
        float humidity;
        float vy;        // Vertical velocity
        float viscosityDamping; // EMA of chaos-driven viscosity modulation
    };

    /**
     * @brief Per-cell configuration derived from biome and terrain.
     */
    struct LbmCellConfig {
        float drag;
        glm::vec4 aerosolReleaseRates;
        float sensibleHeatFactor;
        float terrainHeight;
    };

    /**
     * @brief Physically based output of the weather system.
     * Consolidates all derived atmospheric parameters.
     */
    /**
     * @brief Matching structure for WindCell in shaders/helpers/wind.glsl
     */
    struct WindCellGpu {
        glm::vec4 velocityDrag; // xyz = velocity, w = drag
    };

    /**
     * @brief Matching structure for WindData UBO (std140)
     * Metadata for the wind texture.
     */
    struct WindDataUbo {
        glm::ivec4 originSize;     // x, z = origin, y = width, w = height
        glm::vec4  params;         // x = spacing, y = time, z = curlScale, w = curlStrength
        glm::vec4  cloudAdvection; // xy = horizontal advection, zw = unused
    };

    struct PhysicallyBasedWeatherOutput {
        // Basic Physics
        glm::vec2 windVelocity;   // Horizontal wind (from LBM rho/u)
        float verticalWind;       // Vertical wind (from LBM vy)
        float temperature;        // Absolute temperature in Kelvin
        float pressure;           // Air pressure in hPa
        float humidity;           // Relative humidity (0.0 - 1.0)

        // Scattering & Atmosphere
        glm::vec3 rayleighScattering;
        float mieScattering;
        float mieExtinction;
        glm::vec3 aerosolColor;

        // Clouds
        float cloudCoverage;
        float cloudDensity;
        float cloudAltitude;
        float cloudThickness;

        // Global context
        float timeOfDay;          // 0.0 - 24.0
    };

    struct LbmSnapshot {
        PhysicallyBasedWeatherOutput output;
        std::vector<glm::vec4>       windData;
        std::vector<glm::vec4>       scalarData; // x: temp, y: humidity, z: pressure, w: viscosityDamping
        std::vector<glm::vec4>       aerosolData; // x,y,z,w: concentrations of 4 aerosol types
        WindDataUbo                  uboMetadata;
        glm::ivec2                   gridAnchor;
        bool                         valid = false;
    };

    enum class LbmInjectionType { Pressure, Aerosol, Temperature };

    struct LbmInjection {
        LbmInjectionType type;
        glm::vec3        pos;
        float            value1;
        float            value2;
    };

} // namespace Boidsish
