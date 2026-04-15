#include "weather_lbm_simulator.h"
#include <cmath>
#include <algorithm>
#include "Simplex.h"
#include "biome_properties.h"
#include "logger.h"
#include <vector>

namespace Boidsish {

    const int WeatherLbmSimulator::cx[9] = { 0, 1, 0, -1, 0, 1, -1, -1, 1 };
    const int WeatherLbmSimulator::cz[9] = { 0, 0, 1, 0, -1, 1, 1, -1, -1 };
    const float WeatherLbmSimulator::weights[9] = {
        4.0f/9.0f,
        1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f,
        1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f
    };

    WeatherLbmSimulator::WeatherLbmSimulator(int width, int height)
        : width_(width), height_(height) {
        grid1_.resize(width * height);
        grid2_.resize(width * height);
        currentGrid_ = &grid1_;
        nextGrid_ = &grid2_;
        config_.resize(width * height);

        // Initial defaults
        for (auto& cell : *currentGrid_) {
            cell.temperature = 288.15f; // 15C
            cell.aerosol = 0.01f;
            cell.vy = 0.0f;
            for (int i = 0; i < 9; ++i) {
                cell.f[i] = weights[i]; // rho = 1.0, u = 0
            }
        }
        *nextGrid_ = *currentGrid_;
    }

    WeatherLbmSimulator::~WeatherLbmSimulator() {}

    void WeatherLbmSimulator::Update(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain, const glm::vec3& cameraPos) {
        if (!initialized_) {
            Initialize(terrain);
            initialized_ = true;
        }

        // Anchor grid to camera chunk position
        glm::ivec2 newAnchor;
        newAnchor.x = (int)std::floor(cameraPos.x / 32.0f) - width_ / 2;
        newAnchor.y = (int)std::floor(cameraPos.z / 32.0f) - height_ / 2;

        if (newAnchor != gridAnchor_) {
            glm::ivec2 shiftOffset = newAnchor - gridAnchor_;
            ShiftGrid(shiftOffset);
            gridAnchor_ = newAnchor;
        }

        UpdateConfig(terrain);

        // Fixed timestep loop
        accumulator_ += deltaTime;
        while (accumulator_ >= dt_) {
            Step(dt_, totalTime, timeOfDay, terrain);
            accumulator_ -= dt_;
        }

        DeriveAtmosphere(timeOfDay);
    }

    void WeatherLbmSimulator::Initialize(const ITerrainGenerator& terrain) {
        // Perturb initial state with noise
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                int idx = z * width_ + x;
                float worldX = (float)(x + gridAnchor_.x) * 32.0f;
                float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

                float n = Simplex::noise(glm::vec2(worldX * 0.001f, worldZ * 0.001f));
                (*currentGrid_)[idx].temperature += n * 5.0f;
                (*currentGrid_)[idx].aerosol += std::abs(n) * 0.05f;

                // Set initial f based on noise-driven wind
                glm::vec2 u(Simplex::noise(glm::vec2(worldX * 0.002f, worldZ * 0.002f)),
                             Simplex::noise(glm::vec2(worldX * 0.002f + 100.0f, worldZ * 0.002f + 100.0f)));
                u *= 0.1f;

                for (int i = 0; i < 9; ++i) {
                    (*currentGrid_)[idx].f[i] = CalculateEquilibrium(i, 1.0f, u);
                }
            }
        }
        *nextGrid_ = *currentGrid_;
    }

    float WeatherLbmSimulator::CalculateEquilibrium(int i, float rho, glm::vec2 u) {
        float cu = (float)cx[i] * u.x + (float)cz[i] * u.y;
        float u2 = u.x * u.x + u.y * u.y;
        return weights[i] * rho * (1.0f + 3.0f * cu + 4.5f * cu * cu - 1.5f * u2);
    }

    void WeatherLbmSimulator::CollisionAndStreaming() {
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                int idx = z * width_ + x;
                LbmCell& cell = (*currentGrid_)[idx];

                // 1. Macros
                float rho = 0.0f;
                glm::vec2 u(0.0f);
                for (int i = 0; i < 9; ++i) {
                    rho += cell.f[i];
                    u.x += cell.f[i] * (float)cx[i];
                    u.y += cell.f[i] * (float)cz[i];
                }
                if (rho > 0.0f) u /= rho;

                // If vy is positive (updraft), air leaves the horizontal plane, reducing density.
                // If vy is negative (downdraft), air hits the ground and spreads, increasing density.
                const float massTransferRate = 0.05f; // Tune this based on how aggressive you want the wind
                float dRho = -cell.vy * massTransferRate * dt_;

                // Apply density change proportionally across the distributions,
                // clamping to prevent vacuum collapse numerical instability.
                if (rho + dRho > 0.5f && rho + dRho < 2.0f) {
                    rho += dRho;
                    for (int i = 0; i < 9; ++i) {
                        // Distribute the mass loss/gain evenly weighted
                        cell.f[i] += dRho * weights[i];
                    }
                }

                // 2. Collision & Streaming
                for (int i = 0; i < 9; ++i) {
                    float feq = CalculateEquilibrium(i, rho, u);
                    float f_post = cell.f[i] - omega_ * (cell.f[i] - feq);

                    int nx = (x + cx[i] + width_) % width_;
                    int nz = (z + cz[i] + height_) % height_;
                    (*nextGrid_)[nz * width_ + nx].f[i] = f_post;
                }

                // Scalar transport - Semi-Lagrangian
                glm::vec2 p_back = glm::vec2(x, z) - u * 1.0f; // dt is implicit here for LBM lattice
                int x0 = (int)std::floor(p_back.x);
                int z0 = (int)std::floor(p_back.y);
                float tx = p_back.x - x0;
                float tz = p_back.y - z0;

                auto sample = [&](int sx, int sz) -> const LbmCell& {
                    sx = (sx + width_) % width_;
                    sz = (sz + height_) % height_;
                    return (*currentGrid_)[sz * width_ + sx];
                };

                const LbmCell& c00 = sample(x0, z0);
                const LbmCell& c10 = sample(x0 + 1, z0);
                const LbmCell& c01 = sample(x0, z0 + 1);
                const LbmCell& c11 = sample(x0 + 1, z0 + 1);

                LbmCell& nextCell = (*nextGrid_)[idx];

            // Inside CollisionAndStreaming's scalar transport phase:
                float temp_interp = glm::mix(glm::mix(c00.temperature, c10.temperature, tx),
                                            glm::mix(c01.temperature, c11.temperature, tx), tz);

                // Optional: add a tiny bit of anti-diffusion (sharpening) for thermals
                // This pulls the interpolated temperature slightly closer to the current cell's
                // original temperature if the difference is small, preserving sharp thermal columns.
                float diff = cell.temperature - temp_interp;
                if (std::abs(diff) < 0.5f) {
                    temp_interp += diff * 0.1f;
                }

                nextCell.temperature = temp_interp;

                // nextCell.temperature = glm::mix(glm::mix(c00.temperature, c10.temperature, tx),
                //                                glm::mix(c01.temperature, c11.temperature, tx), tz);

                nextCell.aerosol = glm::mix(glm::mix(c00.aerosol, c10.aerosol, tx),
                                           glm::mix(c01.aerosol, c11.aerosol, tx), tz);

                nextCell.vy = glm::mix(glm::mix(c00.vy, c10.vy, tx),
                                      glm::mix(c01.vy, c11.vy, tx), tz);
            }
        }
        std::swap(currentGrid_, nextGrid_);
    }

    void WeatherLbmSimulator::UpdateConfig(const ITerrainGenerator& terrain) {
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                int idx = z * width_ + x;
                float worldX = (float)(x + gridAnchor_.x) * 32.0f;
                float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

                auto [height, normal] = terrain.GetTerrainPropertiesAtPoint(worldX, worldZ);
                float control = terrain.GetBiomeControlValue(worldX, worldZ);

                int low_idx;
                float t;
                terrain.GetBiomeIndicesAndWeights(control, low_idx, t);
                int high_idx = std::min(low_idx + 1, (int)kBiomes.size() - 1);

                const auto& b1 = kBiomes[low_idx];
                const auto& b2 = kBiomes[high_idx];

                config_[idx].drag = glm::mix(b1.dragFactor, b2.dragFactor, t);
                config_[idx].aerosolReleaseRate = glm::mix(b1.aerosolReleaseRate, b2.aerosolReleaseRate, t);
                config_[idx].sensibleHeatFactor = glm::mix(b1.sensibleHeatFactor, b2.sensibleHeatFactor, t);
                config_[idx].aerosolColor = glm::mix(b1.aerosolColor, b2.aerosolColor, t);
                config_[idx].terrainHeight = height;
            }
        }
    }

    void WeatherLbmSimulator::Step(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain) {
        CollisionAndStreaming();
        ApplyPhysics(deltaTime, totalTime, timeOfDay);
        ApplyBoundaries(totalTime);
    }

    void WeatherLbmSimulator::ApplyPhysics(float deltaTime, float totalTime, float timeOfDay) {
        const float PI = 3.14159265358979323846f;
        float solarAngle = (timeOfDay - 14.0f) * (PI / 12.0f);
        float baseTemp = 288.15f + 10.0f * std::cos(solarAngle);

        // Rate at which temperature normalizes to ambient (tune this).
        // A smaller number means the terrain holds heat longer into the evening.
        const float coolingRelaxation = 0.02f;

        for (int i = 0; i < (int)currentGrid_->size(); ++i) {
            LbmCell& cell = (*currentGrid_)[i];
            const LbmCellConfig& cfg = config_[i];

            // 1. Radiative Cooling / Atmospheric Mixing
            cell.temperature += (baseTemp - cell.temperature) * coolingRelaxation;

            // 2. Sensible Heat Flux (Solar heating)
            float heating = std::max(0.0f, std::cos(solarAngle)) * cfg.sensibleHeatFactor * 0.1f;
            cell.temperature += heating;

            // 3. Boussinesq Buoyancy
            // Temperature difference drives vertical velocity
            float tempDiff = cell.temperature - baseTemp;
            float buoyancy = tempDiff * 0.001f;
            cell.vy += buoyancy;

            // 4. Drag coupling (Terrain & Biome)
            // Height-based drag + biome roughness
            float heightDrag = std::max(0.0f, cfg.terrainHeight) * 0.0001f;
            float totalDrag = (cfg.drag * 0.05f) + heightDrag;

            // Apply drag to velocities
            cell.vy *= (1.0f - totalDrag);
            for (int j = 0; j < 9; ++j) {
                if (j > 0) { // Don't apply to stationary f0
                    cell.f[j] *= (1.0f - totalDrag * 0.1f);
                }
            }

            // 5. Aerosol Release
            cell.aerosol += cfg.aerosolReleaseRate * 0.001f;
            // Diffusion / Decay
            cell.aerosol *= 0.99f;

            // Thermal rising effects horizontal wind (divergence)
            // If vy is high, it "sucks" air in (simplified)
            glm::vec2 inflow(0.0f);
            if (cell.vy > 0.01f) {
                // This would normally be handled by the pressure term in LBM,
                // but we can add a small force towards high-buoyancy areas.
            }
        }
    }

    void WeatherLbmSimulator::ApplyBoundaries(float totalTime) {
        // Apply noise-driven wind at the edges
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                if (x == 0 || x == width_ - 1 || z == 0 || z == height_ - 1) {
                    int idx = z * width_ + x;
                    float worldX = (float)(x + gridAnchor_.x) * 32.0f;
                    float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

                    glm::vec2 targetU(
                        Simplex::noise(glm::vec2(worldX * 0.001f + totalTime * 0.1f, worldZ * 0.001f)),
                        Simplex::noise(glm::vec2(worldX * 0.001f + 500.0f, worldZ * 0.001f + totalTime * 0.1f))
                    );
                    targetU *= 0.2f;

                    // Force boundary equilibrium
                    for (int i = 0; i < 9; ++i) {
                        (*currentGrid_)[idx].f[i] = CalculateEquilibrium(i, 1.0f, targetU);
                    }
                }
            }
        }
    }

    const LbmCell* WeatherLbmSimulator::GetCellAtPosition(const glm::vec3& pos) const {
        int chunkX = (int)std::floor(pos.x / 32.0f);
        int chunkZ = (int)std::floor(pos.z / 32.0f);

        int x = chunkX - gridAnchor_.x;
        int z = chunkZ - gridAnchor_.y;

        if (x < 0 || x >= width_ || z < 0 || z >= height_) {
            return nullptr;
        }

        return &((*currentGrid_)[z * width_ + x]);
    }

    void WeatherLbmSimulator::PopulateWindData(WindDataUbo& ubo, float totalTime, float curlScale, float curlStrength) const {
        ubo.originSize = glm::ivec4(gridAnchor_.x, gridAnchor_.y, width_, height_);
        ubo.params = glm::vec4(32.0f, totalTime, curlScale, curlStrength);

        for (int i = 0; i < width_ * height_ && i < 3840; ++i) {
            const LbmCell& cell = (*currentGrid_)[i];
            const LbmCellConfig& cfg = config_[i];

            float rho = 0.0f;
            glm::vec2 u(0.0f);
            for (int j = 0; j < 9; ++j) {
                rho += cell.f[j];
                u.x += cell.f[j] * (float)cx[j];
                u.y += cell.f[j] * (float)cz[j];
            }
            if (rho > 0.0f) u /= rho;

            ubo.grid[i].velocityDrag = glm::vec4(u.x, cell.vy, u.y, cfg.drag);
        }
    }

    PhysicallyBasedWeatherOutput WeatherLbmSimulator::GetWeatherAtPosition(const glm::vec3& pos) const {
        // For now, return global output but update wind from local cell
        PhysicallyBasedWeatherOutput out = currentOutput_;
        const LbmCell* cell = GetCellAtPosition(pos);
        if (cell) {
            float rho = 0.0f;
            glm::vec2 u(0.0f);
            for (int i = 0; i < 9; ++i) {
                rho += cell->f[i];
                u.x += cell->f[i] * (float)cx[i];
                u.y += cell->f[i] * (float)cz[i];
            }
            if (rho > 0.0f) u /= rho;
            out.windVelocity = u;
            out.verticalWind = cell->vy;
            out.temperature = cell->temperature;
        }
        return out;
    }

    void WeatherLbmSimulator::DeriveAtmosphere(float timeOfDay) {
        const float PI = 3.14159265358979323846f;
        // Average the simulation state for global weather output
        float avgRho = 0.0f;
        glm::vec2 avgU(0.0f);
        float avgTemp = 0.0f;
        float avgAerosol = 0.0f;
        float avgVy = 0.0f;
        glm::vec3 avgAerosolColor(0.0f);

        for (int i = 0; i < (int)currentGrid_->size(); ++i) {
            float rho = 0.0f;
            glm::vec2 u(0.0f);
            for (int j = 0; j < 9; ++j) {
                rho += (*currentGrid_)[i].f[j];
                u.x += (*currentGrid_)[i].f[j] * (float)cx[j];
                u.y += (*currentGrid_)[i].f[j] * (float)cz[j];
            }
            if (rho > 0.0f) u /= rho;

            avgRho += rho;
            avgU += u;
            avgTemp += (*currentGrid_)[i].temperature;
            avgAerosol += (*currentGrid_)[i].aerosol;
            avgVy += (*currentGrid_)[i].vy;
            avgAerosolColor += config_[i].aerosolColor;
        }

        float n = (float)currentGrid_->size();
        avgRho /= n;
        avgU /= n;
        avgTemp /= n;
        avgAerosol /= n;
        avgVy /= n;
        avgAerosolColor /= n;

        currentOutput_.windVelocity = avgU;
        currentOutput_.verticalWind = avgVy;
        currentOutput_.temperature = avgTemp;
        currentOutput_.timeOfDay = timeOfDay;

        // 1. Pressure derivation (Hydrostatic + LBM rho)
        // P = P0 * exp(-Mgh/RT) - highly simplified
        currentOutput_.pressure = 1013.25f * avgRho;

        // 2. Humidity derivation (Bolton's formula for saturation vapor pressure)
        // es = 6.112 * exp(17.67 * Tc / (Tc + 243.5))
        float tc = avgTemp - 273.15f;
        float es = 6.112f * std::exp(17.67f * tc / (tc + 243.5f));
        // Simple heuristic for actual vapor pressure based on aerosol (proxy for condensation nuclei)
        float e = es * (0.4f + 0.3f * std::sin(timeOfDay * PI / 12.0f) + avgAerosol * 2.0f);
        currentOutput_.humidity = std::clamp(e / es, 0.0f, 1.0f);

        // 3. Rayleigh Scattering (P/T dependent)
        // BetaR = BetaR0 * (P/P0) * (T0/T)
        float pt_factor = (currentOutput_.pressure / 1013.25f) * (288.15f / avgTemp);
        currentOutput_.rayleighScattering = glm::vec3(5.802e-3f, 13.558e-3f, 33.100e-3f) * pt_factor;

        // 4. Mie Scattering (Aerosol dependent)
        currentOutput_.mieScattering = 3.996e-3f + avgAerosol * 0.1f;
        currentOutput_.mieExtinction = currentOutput_.mieScattering * 1.11f;
        currentOutput_.aerosolColor = avgAerosolColor;

        // 5. Cloud Heuristics
        // Clouds form when humidity is high and there's upward motion
        float cloudPotential = std::max(0.0f, currentOutput_.humidity - 0.7f) * 3.33f;
        cloudPotential += std::max(0.0f, avgVy) * 10.0f;

        currentOutput_.cloudCoverage = std::clamp(cloudPotential * 0.5f, 0.0f, 1.0f);
        currentOutput_.cloudDensity = std::clamp(cloudPotential, 0.0f, 1.0f);
        currentOutput_.cloudAltitude = 400.0f + 200.0f * (1.0f - currentOutput_.humidity);
        currentOutput_.cloudThickness = 100.0f + 400.0f * currentOutput_.cloudDensity;
    }

    void WeatherLbmSimulator::ShiftGrid(glm::ivec2 shiftOffset) {
        if (shiftOffset.x == 0 && shiftOffset.y == 0) return;

        std::vector<LbmCell> tempGrid(width_ * height_);

        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                // Source index relative to the shift
                int srcX = x + shiftOffset.x;
                int srcZ = z + shiftOffset.y;

                int dstIdx = z * width_ + x;

                // If the source is within the old grid bounds, copy it over
                if (srcX >= 0 && srcX < width_ && srcZ >= 0 && srcZ < height_) {
                    tempGrid[dstIdx] = (*currentGrid_)[srcZ * width_ + srcX];
                } else {
                    // This is a newly revealed edge cell; initialize it to ambient
                    tempGrid[dstIdx].temperature = 288.15f;
                    tempGrid[dstIdx].aerosol = 0.01f;
                    tempGrid[dstIdx].vy = 0.0f;
                    for (int i = 0; i < 9; ++i) {
                        tempGrid[dstIdx].f[i] = weights[i]; // rho = 1.0, u = 0.0
                    }
                }
            }
        }

        *currentGrid_ = tempGrid;
        *nextGrid_ = tempGrid; // Sync both buffers to prevent smearing next frame
    }

} // namespace Boidsish
