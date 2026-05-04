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
        : width_(width + 2 * kPadding), height_(height + 2 * kPadding) {
        grid1_.resize(width_ * height_);
        grid2_.resize(width_ * height_);
        currentGrid_ = &grid1_;
        nextGrid_ = &grid2_;
        config_.resize(width_ * height_);

        // Initial defaults
        for (auto& cell : *currentGrid_) {
            cell.temperature = 288.15f; // 15C
            cell.aerosol = 0.01f;
            cell.humidity = 0.5f;
            cell.vy = 0.0f;
            for (int i = 0; i < 9; ++i) {
                cell.f[i] = weights[i]; // rho = 1.0, u = 0
            }
        }
        *nextGrid_ = *currentGrid_;
    }

    WeatherLbmSimulator::~WeatherLbmSimulator() {}

	void WeatherLbmSimulator::UpdateAnchor(const glm::vec3& cameraPos, float totalTime, float timeOfDay) {
		// Anchor grid to camera chunk position
		glm::ivec2 newAnchor;
		newAnchor.x = (int)std::floor(cameraPos.x / 32.0f) - width_ / 2;
		newAnchor.y = (int)std::floor(cameraPos.z / 32.0f) - height_ / 2;

		if (newAnchor != gridAnchor_) {
			glm::ivec2 shiftOffset = newAnchor - gridAnchor_;

            // If the shift is too large, it's better to just re-initialize the whole grid
            // to prevent massive artifacts from a "teleport".
            if (std::abs(shiftOffset.x) > width_ / 4 || std::abs(shiftOffset.y) > height_ / 4) {
                gridAnchor_ = newAnchor;
                initialized_ = false;
                return;
            }

			// CRITICAL: Update anchor BEFORE shifting so newly revealed cells know their world position
			gridAnchor_ = newAnchor;
			ShiftGrid(shiftOffset, totalTime, timeOfDay);
		}
	}

	void WeatherLbmSimulator::Update(float deltaTime, float totalTime, float timeOfDay, const ITerrainGenerator& terrain, const glm::vec3& cameraPos, float windSpeed, float windStrength, float targetTemp, float targetPressure, float targetHumidity) {
		if (!initialized_) {
			Initialize(terrain, totalTime, timeOfDay);
			initialized_ = true;
		}

		UpdateAnchor(cameraPos, totalTime, timeOfDay);

        if (!initialized_) {
            // If UpdateAnchor reset initialized_, re-run Initialize
            Initialize(terrain, totalTime, timeOfDay);
            initialized_ = true;
        }

		UpdateConfig(terrain);

        // Fixed timestep loop
        accumulator_ += deltaTime;
        while (accumulator_ >= dt_) {
            CollisionAndStreaming();
            ApplyPhysics(dt_, totalTime, timeOfDay);
            ApplyBoundaries(totalTime, windSpeed, windStrength, timeOfDay, targetTemp, targetPressure, targetHumidity);
            accumulator_ -= dt_;
        }

        DeriveAtmosphere(totalTime, timeOfDay);
    }

    void WeatherLbmSimulator::Initialize(const ITerrainGenerator& terrain, float totalTime, float timeOfDay) {
        // Perturb initial state with noise
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                InitializeCell(x, z, totalTime, timeOfDay, (*currentGrid_)[z * width_ + x]);
            }
        }
        *nextGrid_ = *currentGrid_;
    }

    void WeatherLbmSimulator::InitializeCell(int x, int z, float totalTime, float timeOfDay, LbmCell& cell) {
        float worldX = (float)(x + gridAnchor_.x) * 32.0f;
        float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

        float baseTemp = GetBaseTemperature(worldX, worldZ, totalTime, timeOfDay);

        // Bias towards global averages if initialized
        float targetTemp = initialized_ ? currentOutput_.temperature : baseTemp;
        float targetHumidity = initialized_ ? currentOutput_.humidity : 0.5f;
        float targetRho = initialized_ ? currentOutput_.pressure / 1013.25f : 1.0f;

        float n = Simplex::noise(glm::vec2(worldX * 0.001f, worldZ * 0.001f));
        cell.temperature = glm::mix(targetTemp, baseTemp, 0.5f) + n * 5.0f;
        cell.aerosol = 0.01f + std::abs(n) * 0.05f;
        cell.humidity = glm::mix(targetHumidity, 0.4f + std::abs(n) * 0.2f, 0.5f);
        cell.vy = 0.0f;

        // Set initial f based on noise-driven wind
        glm::vec2 u(Simplex::noise(glm::vec2(worldX * 0.002f, worldZ * 0.002f)),
                             Simplex::noise(glm::vec2(worldX * 0.002f + 100.0f, worldZ * 0.002f + 100.0f)));
        u *= 0.1f;

        for (int i = 0; i < 9; ++i) {
            cell.f[i] = CalculateEquilibrium(i, targetRho, u);
        }
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
                if (rho > 1e-6f) u /= rho;

                // NaN safety: Reset cell if it explodes
                if (std::isnan(rho) || rho < 0.1f || rho > 10.0f) {
                    rho = 1.0f;
                    u = glm::vec2(0.0f);
                    cell.temperature = 288.15f;
                    cell.aerosol = 0.01f;
                    cell.humidity = 0.5f;
                    cell.vy = 0.0f;
                    for (int i = 0; i < 9; ++i) cell.f[i] = weights[i];
                }

                // If vy is positive (updraft), air leaves the horizontal plane, reducing density.
                // If vy is negative (downdraft), air hits the ground and spreads, increasing density.
                const float massTransferRate = 0.05f;
                float dRho = glm::clamp(-cell.vy * massTransferRate * dt_, -0.1f, 0.1f);

                auto lenSq = glm::dot(u, u);
                if (lenSq >= 0.09f) {
                    u *= 0.3f * glm::inversesqrt(lenSq);
                }

                // Apply density change proportionally across the distributions,
                // clamping to prevent vacuum collapse numerical instability.
                if (rho + dRho > 0.8f && rho + dRho < 1.2f) {
                    rho += dRho;
                    for (int i = 0; i < 9; ++i) {
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

                nextCell.humidity = glm::mix(glm::mix(c00.humidity, c10.humidity, tx),
                                            glm::mix(c01.humidity, c11.humidity, tx), tz);

                nextCell.vy = glm::mix(glm::mix(c00.vy, c10.vy, tx),
                                      glm::mix(c01.vy, c11.vy, tx), tz);

                nextCell.vy *= glm::mix(0.95f, 0.75f, glm::smoothstep(0.0f, 1.0f, nextCell.vy));
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


    void WeatherLbmSimulator::ApplyPhysics(float deltaTime, float totalTime, float timeOfDay) {
        const float PI = 3.14159265358979323846f;
        float seasonalIntensity = 0.5f + 0.5f * GetSeasonalFactor(totalTime); // 0.5 to 1.0

        // Rate at which temperature normalizes to ambient (tune this).
        // A smaller number means the terrain holds heat longer into the evening.
        const float coolingRelaxation = 0.02f;

        for (int i = 0; i < (int)currentGrid_->size(); ++i) {
            LbmCell& cell = (*currentGrid_)[i];
            const LbmCellConfig& cfg = config_[i];

            int x = i % width_;
            int z = i / width_;
            float worldX = (float)(x + gridAnchor_.x) * 32.0f;
            float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

            float baseTemp = GetBaseTemperature(worldX, worldZ, totalTime, timeOfDay);

            // 1. Radiative Cooling / Atmospheric Mixing
            cell.temperature += (baseTemp - cell.temperature) * coolingRelaxation;

            // 2. Sensible Heat Flux (Solar heating)
            float solarAngle = (timeOfDay + (worldX * 0.001f) - 14.0f) * (PI / 12.0f);
            float heating = std::max(0.0f, std::cos(solarAngle)) * cfg.sensibleHeatFactor * 0.1f * seasonalIntensity;
            cell.temperature += heating;

            // 3. Boussinesq Buoyancy
            // Temperature difference drives vertical velocity
            float tempDiff = cell.temperature - baseTemp;
            float buoyancy = tempDiff * 0.001f;
            cell.vy += buoyancy;

            // 4. Drag coupling (Terrain & Biome) - Forcing approach
            // We apply a force opposite to the velocity rather than bleeding f components.
            float heightDrag = std::max(0.0f, cfg.terrainHeight) * 0.0001f;
            float totalDrag = (cfg.drag * 0.05f) + heightDrag;

            cell.vy *= (1.0f - totalDrag);

            // Calculate current macro wind
            float rho = 0.0f;
            glm::vec2 u(0.0f);
            for (int j = 0; j < 9; ++j) {
                rho += cell.f[j];
                u.x += cell.f[j] * (float)cx[j];
                u.y += cell.f[j] * (float)cz[j];
            }
            if (rho > 1e-6f) u /= rho;

            // Apply force vector opposite to u
            glm::vec2 force = -u * totalDrag * 0.2f;
            for (int j = 0; j < 9; ++j) {
                float cu = (float)cx[j] * force.x + (float)cz[j] * force.y;
                cell.f[j] += weights[j] * 3.0f * cu;
            }

            // 5. Aerosol Release
            cell.aerosol += cfg.aerosolReleaseRate * 0.001f * seasonalIntensity;
            // Diffusion / Decay
            cell.aerosol *= 0.99f;

            // 6. Humidity modeling (Evaporation & Condensation)
            // Evaporation based on sensible heat factor (proxy for ground moisture availability) and temperature
            float tc_local = cell.temperature - 273.15f;
            float evaporation = std::max(0.0f, tc_local) * cfg.sensibleHeatFactor * 0.0001f;
            cell.humidity += evaporation;

            // Saturation check (Bolton's formula simplified)
            float es_local = 6.112f * std::exp(17.67f * tc_local / (tc_local + 243.5f));
            // In our simulation, humidity 1.0 = es_local.
            // If it goes significantly above 1.0, it should condense.
            if (cell.humidity > 1.0f) {
                float excess = cell.humidity - 1.0f;
                cell.humidity -= excess * 0.1f; // Sink to precipitation
            }
            cell.humidity = std::clamp(cell.humidity, 0.0f, 1.2f);

            // Thermal rising effects horizontal wind (divergence)
            // If vy is high, it "sucks" air in (simplified)
            glm::vec2 inflow(0.0f);
            if (cell.vy > 0.01f) {
                // This would normally be handled by the pressure term in LBM,
                // but we can add a small force towards high-buoyancy areas.
            }
        }
    }

    void WeatherLbmSimulator::ApplyBoundaries(float totalTime, float windSpeed, float windStrength, float timeOfDay, float targetTemp, float targetPressure, float targetHumidity) {
        float targetRho = targetPressure / 1013.25f;

        // Apply noise-driven wind at the edges with sponge layer dampening
        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                // Calculate distance to nearest edge
                int dist = std::min({x, width_ - 1 - x, z, height_ - 1 - z});

                if (dist < kPadding) {
                    int idx = z * width_ + x;
                    float worldX = (float)(x + gridAnchor_.x) * 32.0f;
                    float worldZ = (float)(z + gridAnchor_.y) * 32.0f;

                    // Prevailing wind based on spatial gradient
                    glm::vec2 prevailingWind(0.02f, 0.01f); // Baseline flow
                    prevailingWind += glm::vec2(worldX + worldZ, worldX - worldZ) * 0.0001f;
                    prevailingWind = glm::normalize(prevailingWind);

                    auto noiseWind = Simplex::curlNoise(glm::vec2(worldX * 0.001f + totalTime * windSpeed, worldZ * 0.001f + totalTime * windSpeed), atan2(prevailingWind.x, prevailingWind.y));

                    glm::vec2 targetU = prevailingWind + noiseWind * 0.1f;
                    targetU *= Simplex::noise({worldX, worldZ}) * 0.5f + 0.5f;

                    // Lattice velocity MUST stay below ~0.15 for stability
                    targetU = glm::clamp(targetU * (0.05f + windStrength * 5.0f), -0.14f, 0.14f);

                    float baseTemp = GetBaseTemperature(worldX, worldZ, totalTime, timeOfDay);
                    float finalTargetTemp = glm::mix(baseTemp, targetTemp, 0.5f);

                    // Sponge layer relaxation factor: 1.0 at edge, 0.0 at interior
                    float spongeFactor = 1.0f - (float)dist / (float)kPadding;
                    // Stronger dampening closer to edge
                    float blend = std::pow(spongeFactor, 2.0f);

                    // Force target distribution at the boundary, blend in sponge layer
                    for (int i = 0; i < 9; ++i) {
                        float f_eq = CalculateEquilibrium(i, targetRho, targetU);
                        (*currentGrid_)[idx].f[i] = glm::mix((*currentGrid_)[idx].f[i], f_eq, blend);
                    }

                    (*currentGrid_)[idx].temperature = glm::mix((*currentGrid_)[idx].temperature, finalTargetTemp, blend * 0.1f);
                    (*currentGrid_)[idx].humidity = glm::mix((*currentGrid_)[idx].humidity, targetHumidity, blend * 0.1f);

                    // Dampen vertical velocity in sponge layer to prevent shocks
                    (*currentGrid_)[idx].vy *= (1.0f - blend * 0.5f);
                }
            }
        }
    }

    float WeatherLbmSimulator::GetSeasonalFactor(float totalTime) const {
        const float seasonalPeriod = 7200.0f; // 2 hours for full cycle
        return 0.5f + 0.5f * std::cos(totalTime * (2.0f * 3.14159265f / seasonalPeriod));
    }

    float WeatherLbmSimulator::GetBaseTemperature(float worldX, float worldZ, float totalTime, float timeOfDay) const {
        const float PI = 3.14159265358979323846f;

        // 1. Seasonal variation (0.0 to 1.0)
        float seasonalFactor = GetSeasonalFactor(totalTime);
        float seasonalTempOffset = (seasonalFactor - 0.5f) * 25.0f; // +/- 12.5C

        // 2. Diurnal variation with longitudinal offset
        // "east side pegged to be ahead of the west side"
        float longitudeOffset = worldX * 0.001f;
        float localTimeOfDay = timeOfDay + longitudeOffset;

        float solarAngle = (localTimeOfDay - 14.0f) * (PI / 12.0f);
        float diurnalTemp = 12.0f * std::cos(solarAngle);

        // 3. Spatial gradient (slanted midpoint)
        // A prevailing gradient that makes things generally colder towards "North-West" (negative X, positive Z)
        float spatialGradient = (worldX - worldZ) * 0.02f;

        float baseTemp = 285.15f + seasonalTempOffset + diurnalTemp + spatialGradient;
        return baseTemp;
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

    void WeatherLbmSimulator::InjectPressure(const glm::vec3& pos, float pressureHpa, float burstStrength) {
        int chunkX = (int)std::floor(pos.x / 32.0f);
        int chunkZ = (int)std::floor(pos.z / 32.0f);

        int gx = chunkX - gridAnchor_.x;
        int gz = chunkZ - gridAnchor_.y;

        if (gx < 0 || gx >= width_ || gz < 0 || gz >= height_) return;

        float targetRho = pressureHpa / 1013.25f;

        auto inject = [&](int x, int z, float rho, glm::vec2 u) {
            if (x < 0 || x >= width_ || z < 0 || z >= height_) return;
            int idx = z * width_ + x;
            for (int i = 0; i < 9; ++i) {
                float val = CalculateEquilibrium(i, rho, u);
                (*currentGrid_)[idx].f[i] = val;
                (*nextGrid_)[idx].f[i] = val;
            }
        };

        // Set center cell
        inject(gx, gz, targetRho, glm::vec2(0.0f));

        // Prime neighbors if burst is requested
        if (burstStrength > 0.0f) {
            for (int i = 1; i < 9; ++i) {
                int nx = gx + cx[i];
                int nz = gz + cz[i];
                glm::vec2 outwardU = glm::normalize(glm::vec2(cx[i], cz[i])) * burstStrength;
                inject(nx, nz, 1.0f, outwardU);
            }
        }
    }

    void WeatherLbmSimulator::InjectAerosol(const glm::vec3& pos, float concentration) {
        int chunkX = (int)std::floor(pos.x / 32.0f);
        int chunkZ = (int)std::floor(pos.z / 32.0f);

        int gx = chunkX - gridAnchor_.x;
        int gz = chunkZ - gridAnchor_.y;

        if (gx < 0 || gx >= width_ || gz < 0 || gz >= height_) return;

        int idx = gz * width_ + gx;
        (*currentGrid_)[idx].aerosol = concentration;
        (*nextGrid_)[idx].aerosol = concentration;
    }

    void WeatherLbmSimulator::InjectTemperature(const glm::vec3& pos, float temperatureK) {
        int chunkX = (int)std::floor(pos.x / 32.0f);
        int chunkZ = (int)std::floor(pos.z / 32.0f);

        int gx = chunkX - gridAnchor_.x;
        int gz = chunkZ - gridAnchor_.y;

        if (gx < 0 || gx >= width_ || gz < 0 || gz >= height_) return;

        int idx = gz * width_ + gx;
        (*currentGrid_)[idx].temperature = temperatureK;
        (*nextGrid_)[idx].temperature = temperatureK;
    }

    void WeatherLbmSimulator::PopulateWindData(WindDataUbo& ubo, std::vector<glm::vec4>& grid_out, float totalTime, float curlScale, float curlStrength) const {
        float gridSpacing = 32.0f;
        // GLSL: x, z = origin, y = width, w = height
        ubo.originSize = glm::ivec4(gridAnchor_.x, width_, gridAnchor_.y, height_);
        ubo.params = glm::vec4(gridSpacing, totalTime, curlScale, curlStrength);

        // Convert lattice velocity to world velocity (m/s)
        // lattice_u * (spacing / dt)
        float conversion = gridSpacing / dt_;

        if (grid_out.size() < (size_t)(width_ * height_)) {
            grid_out.resize(width_ * height_);
        }

        for (int i = 0; i < width_ * height_; ++i) {
            const LbmCell& cell = (*currentGrid_)[i];
            const LbmCellConfig& cfg = config_[i];

            float rho = 0.0f;
            glm::vec2 u(0.0f);
            for (int j = 0; j < 9; ++j) {
                rho += cell.f[j];
                u.x += cell.f[j] * (float)cx[j];
                u.y += cell.f[j] * (float)cz[j];
            }
            if (rho > 1e-6f) u /= rho;

            grid_out[i] = glm::vec4(u.x * conversion, cell.vy * conversion, u.y * conversion, cfg.drag);
        }
    }

    void WeatherLbmSimulator::TakeSnapshot(LbmSnapshot& snapshot, float totalTime, float curlScale, float curlStrength) const {
        snapshot.output = currentOutput_;
        snapshot.gridAnchor = gridAnchor_;
        PopulateWindData(snapshot.uboMetadata, snapshot.windData, totalTime, curlScale, curlStrength);

        if (snapshot.scalarData.size() < (size_t)(width_ * height_)) {
            snapshot.scalarData.resize(width_ * height_);
        }

        for (int i = 0; i < width_ * height_; ++i) {
            const LbmCell& cell = (*currentGrid_)[i];
            float rho = 0.0f;
            for (int j = 0; j < 9; ++j) rho += cell.f[j];
            snapshot.scalarData[i] = glm::vec4(cell.temperature, cell.humidity, 1013.25f * rho, cell.aerosol);
        }

        snapshot.valid = true;
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
            if (rho > 1e-6f) u /= rho;

            float conversion = 32.0f / dt_;
            out.windVelocity = u * conversion;
            out.verticalWind = cell->vy * conversion;
            out.temperature = cell->temperature;
            out.humidity = cell->humidity;
        }
        return out;
    }

    void WeatherLbmSimulator::DeriveAtmosphere(float totalTime, float timeOfDay) {
        const float PI = 3.14159265358979323846f;
        // Average the simulation state for global weather output
        float avgRho = 0.0f;
        glm::vec2 avgU(0.0f);
        float avgTemp = 0.0f;
        float avgAerosol = 0.0f;
        float avgHumidity = 0.0f;
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
            avgHumidity += (*currentGrid_)[i].humidity;
            avgVy += (*currentGrid_)[i].vy;
            avgAerosolColor += config_[i].aerosolColor;
        }

        float n = (float)currentGrid_->size();
        avgRho /= n;
        avgU /= n;
        avgTemp /= n;
        avgAerosol /= n;
        avgHumidity /= n;
        avgVy /= n;
        avgAerosolColor /= n;

        currentOutput_.windVelocity = avgU;
        currentOutput_.verticalWind = avgVy;
        currentOutput_.temperature = avgTemp;
        currentOutput_.timeOfDay = timeOfDay;

        // 1. Pressure derivation (Hydrostatic + LBM rho)
        // P = P0 * exp(-Mgh/RT) - highly simplified
        currentOutput_.pressure = 1013.25f * avgRho;

        // 2. Humidity derivation
        currentOutput_.humidity = std::clamp(avgHumidity, 0.0f, 1.0f);

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

        // Seasonal cloud factor (more cloudy in "winter"/colder phases)
        float seasonalFactor = GetSeasonalFactor(totalTime);
        cloudPotential += (1.0f - seasonalFactor) * 0.3f;

        currentOutput_.cloudCoverage = std::clamp(cloudPotential * 0.5f, 0.0f, 1.0f);
        currentOutput_.cloudDensity = std::clamp(cloudPotential, 0.0f, 1.0f);
        currentOutput_.cloudAltitude = 400.0f + 200.0f * (1.0f - currentOutput_.humidity);
        currentOutput_.cloudThickness = 100.0f + 400.0f * currentOutput_.cloudDensity;
    }

    void WeatherLbmSimulator::ShiftGrid(glm::ivec2 shiftOffset, float totalTime, float timeOfDay) {
        if (shiftOffset.x == 0 && shiftOffset.y == 0) return;

        std::vector<LbmCell> tempGrid(width_ * height_);

        for (int z = 0; z < height_; ++z) {
            for (int x = 0; x < width_; ++x) {
                int srcX = x + shiftOffset.x;
                int srcZ = z + shiftOffset.y;

                if (srcX >= 0 && srcX < width_ && srcZ >= 0 && srcZ < height_) {
                    tempGrid[z * width_ + x] = (*currentGrid_)[srcZ * width_ + srcX];
                } else {
                    // New cell being moved in, initialize it properly instead of recycling.
                    InitializeCell(x, z, totalTime, timeOfDay, tempGrid[z * width_ + x]);
                }
            }
        }

        *currentGrid_ = tempGrid;
        *nextGrid_ = tempGrid; // Sync both buffers to prevent smearing next frame
    }

} // namespace Boidsish
