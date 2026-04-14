#include "weather_lbm.h"
#include "Simplex.h"
#include <cmath>
#include <algorithm>

namespace Boidsish {

    WeatherLbmSimulator::WeatherLbmSimulator(const WeatherLbmConfig& config)
        : config_(config) {
        int count = config_.width * config_.height;
        cells_.resize(count);
        next_cells_.resize(count);
        obstacles_.resize(count, false);
        cell_configs_.resize(count);

        // Initialize with equilibrium for rho=1, u=0
        for (int i = 0; i < count; ++i) {
            cells_[i].temperature = 0.5f;
            cells_[i].aerosol = 0.0f;
            cells_[i].vz = 0.0f;
            for (int k = 0; k < 9; ++k) {
                cells_[i].f[k] = weights[k];
            }
        }
    }

    WeatherLbmSimulator::~WeatherLbmSimulator() {}

    void WeatherLbmSimulator::Update(float deltaTime, float totalTime) {
        // In a real LBM, dt is usually fixed for stability.
        // We'll assume one LBM step per Update for now, or could sub-step.
        ApplyBoundaries(totalTime);
        CollideAndStream(deltaTime, totalTime);

        // Swap buffers
        std::swap(cells_, next_cells_);
    }

    void WeatherLbmSimulator::SetCell(int x, int y, const LbmCell& cell) {
        if (x >= 0 && x < config_.width && y >= 0 && y < config_.height) {
            cells_[y * config_.width + x] = cell;
        }
    }

    const LbmCell& WeatherLbmSimulator::GetCell(int x, int y) const {
        static LbmCell dummy{};
        if (x >= 0 && x < config_.width && y >= 0 && y < config_.height) {
            return cells_[y * config_.width + x];
        }
        return dummy;
    }

    void WeatherLbmSimulator::SetObstacle(int x, int y, bool is_obstacle) {
        if (x >= 0 && x < config_.width && y >= 0 && y < config_.height) {
            obstacles_[y * config_.width + x] = is_obstacle;
        }
    }

    void WeatherLbmSimulator::SetCellConfig(int x, int y, const LbmCellConfig& config) {
        if (x >= 0 && x < config_.width && y >= 0 && y < config_.height) {
            cell_configs_[y * config_.width + x] = config;
        }
    }

    float WeatherLbmSimulator::GetTemperature(int x, int y) const {
        return GetCell(x, y).temperature;
    }

    float WeatherLbmSimulator::GetAerosol(int x, int y) const {
        return GetCell(x, y).aerosol;
    }

    glm::vec3 WeatherLbmSimulator::GetVelocity(int x, int y) const {
        float     rho;
        glm::vec2 u;
        ComputeMacros(x, y, rho, u);
        return glm::vec3(u.x, u.y, GetCell(x, y).vz);
    }

    float WeatherLbmSimulator::GetPressure(int x, int y) const {
        float     rho;
        glm::vec2 u;
        ComputeMacros(x, y, rho, u);
        return rho; // In LBM pressure is proportional to density
    }

    void WeatherLbmSimulator::ComputeMacros(int x, int y, float& rho, glm::vec2& u) const {
        const LbmCell& cell = cells_[y * config_.width + x];
        rho = 0.0f;
        u = glm::vec2(0.0f);
        for (int k = 0; k < 9; ++k) {
            rho += cell.f[k];
            u.x += cell.f[k] * (float)dx[k];
            u.y += cell.f[k] * (float)dy[k];
        }
        if (rho > 0.0f) {
            u /= rho;
        }
    }

    void WeatherLbmSimulator::CollideAndStream(float dt, float totalTime) {
        int w = config_.width;
        int h = config_.height;

        auto sample = [&](int sx, int sy) {
            int tx = std::clamp(sx, 0, w - 1);
            int ty = std::clamp(sy, 0, h - 1);
            return cells_[ty * w + tx];
        };

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = y * w + x;
                if (obstacles_[idx]) {
                    // Copy obstacle state
                    next_cells_[idx] = cells_[idx];
                    continue;
                }

                float     rho;
                glm::vec2 u;
                ComputeMacros(x, y, rho, u);

                // Safety: clamp rho and u
                rho = std::max(0.1f, rho);
                if (std::isnan(u.x) || std::isnan(u.y))
                    u = glm::vec2(0.0f);

                // Buoyancy and forcing
                float temp = cells_[idx].temperature;
                const LbmCellConfig& cell_config = cell_configs_[idx];

                // Combined drag: global base drag + per-cell roughness
                float total_drag = config_.drag + cell_config.roughness;

                // Vertical buoyancy and drag
                float force_z = config_.buoyancy * (temp - 0.5f) - total_drag * cells_[idx].vz;
                cells_[idx].vz += force_z * dt;
                cells_[idx].vz += config_.thermal_rise * temp * dt;

                // Horizontal coupling: thermal gradient inducing horizontal flow
                // And applying horizontal drag from roughness
                glm::vec2 force_h(0.0f);
                static const int neighbors_h[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& off : neighbors_h) {
                    const LbmCell& n = sample(x + off[0], y + off[1]);
                    force_h.x += (float)off[0] * (cells_[idx].temperature - n.temperature);
                    force_h.y += (float)off[1] * (cells_[idx].temperature - n.temperature);
                }
                force_h *= config_.buoyancy * 0.1f; // Thermal induction
                force_h -= u * total_drag;          // Horizontal drag from roughness

                u += force_h * dt;

                // Collision (BGK) with shifted velocity for forcing
                float feq[9];
                float u2 = u.x * u.x + u.y * u.y;
                for (int k = 0; k < 9; ++k) {
                    float eu = (float)dx[k] * u.x + (float)dy[k] * u.y;
                    feq[k] = weights[k] * rho * (1.0f + 3.0f * eu + 4.5f * eu * eu - 1.5f * u2);
                    cells_[idx].f[k] = cells_[idx].f[k] + config_.omega * (feq[k] - cells_[idx].f[k]);
                }

                // Streaming
                for (int k = 0; k < 9; ++k) {
                    int nx = x + dx[k];
                    int ny = y + dy[k];

                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        int nidx = ny * w + nx;
                        if (obstacles_[nidx]) {
                            static const int noslip[9] = {0, 3, 4, 1, 2, 7, 8, 5, 6};
                            next_cells_[idx].f[noslip[k]] = cells_[idx].f[k];
                        } else {
                            next_cells_[nidx].f[k] = cells_[idx].f[k];
                        }
                    }
                }

                // Advect scalars using semi-lagrangian
                // Using 1.0f as base advection scale for physical consistency
                float src_x = (float)x - u.x * dt;
                float src_y = (float)y - u.y * dt;

                int x0 = std::clamp((int)std::floor(src_x), 0, w - 1);
                int x1 = std::clamp(x0 + 1, 0, w - 1);
                int y0 = std::clamp((int)std::floor(src_y), 0, h - 1);
                int y1 = std::clamp(y0 + 1, 0, h - 1);

                float fx = src_x - std::floor(src_x);
                float fy = src_y - std::floor(src_y);

                auto lerp_cell = [&](float t, const LbmCell& a, const LbmCell& b) {
                    LbmCell res;
                    res.temperature = a.temperature + t * (b.temperature - a.temperature);
                    res.aerosol = a.aerosol + t * (b.aerosol - a.aerosol);
                    res.vz = a.vz + t * (b.vz - a.vz);
                    return res;
                };

                LbmCell r0 = lerp_cell(fx, sample(x0, y0), sample(x1, y0));
                LbmCell r1 = lerp_cell(fx, sample(x0, y1), sample(x1, y1));
                LbmCell final_cell = lerp_cell(fy, r0, r1);

                // Sensible heat transfer: slow down/speed up convergence to advected temperature
                float target_temp = final_cell.temperature;
                next_cells_[idx].temperature = cells_[idx].temperature +
                    (target_temp - cells_[idx].temperature) * std::clamp(cell_config.sensible_heat_factor * dt * 10.0f, 0.0f, 1.0f);

                next_cells_[idx].temperature *= (1.0f - config_.temp_decay * dt);

                // Aerosol release rate: constant source term
                next_cells_[idx].aerosol = final_cell.aerosol + cell_config.aerosol_release_rate * dt;
                next_cells_[idx].aerosol *= (1.0f - config_.aerosol_decay * dt);

                next_cells_[idx].vz = final_cell.vz;

                // Heat and Aerosol Diffusion (simple Laplacian)
                float lapT = 0.0f;
                float lapA = 0.0f;
                static const int neighbors[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
                for (auto& off : neighbors) {
                    const LbmCell& n = sample(x + off[0], y + off[1]);
                    lapT += n.temperature;
                    lapA += n.aerosol;
                }
                lapT = (lapT - 4.0f * cells_[idx].temperature);
                lapA = (lapA - 4.0f * cells_[idx].aerosol);

                float diffusion_coeff = 0.05f; // Small fixed diffusion
                next_cells_[idx].temperature += lapT * diffusion_coeff * dt;
                next_cells_[idx].aerosol += lapA * diffusion_coeff * dt;
            }
        }
    }

    void WeatherLbmSimulator::ApplyBoundaries(float totalTime) {
        int w = config_.width;
        int h = config_.height;

        // Cyclic temperature cycle (Diurnal)
        float diurnal_temp = 0.5f + 0.3f * std::sin(totalTime * 0.1f);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                    int idx = y * w + x;

                    // Noise-adjusted boundaries
                    glm::vec2 noisePos = glm::vec2(x, y) * config_.spatial_scale + glm::vec2(totalTime * config_.time_scale);
                    float     noiseVal = Simplex::noise(noisePos);

                    cells_[idx].temperature = diurnal_temp + 0.1f * noiseVal;

                    // Compute ambient wind from noise
                    glm::vec2 noisePosWind = noisePos * 0.5f;
                    float     noiseAngle = Simplex::noise(noisePosWind + glm::vec2(100.0f)) * 3.14159f;
                    float     noiseSpeed = std::max(0.0f, Simplex::noise(noisePosWind + glm::vec2(200.0f)) * 0.1f);
                    glm::vec2 u(std::cos(noiseAngle) * noiseSpeed, std::sin(noiseAngle) * noiseSpeed);

                    float noiseVz = Simplex::noise(noisePosWind + glm::vec2(300.0f)) * 0.05f;
                    cells_[idx].vz = noiseVz;

                    // Set equilibrium at boundaries
                    float rho = 1.0f;
                    float u2 = u.x * u.x + u.y * u.y;
                    for (int k = 0; k < 9; ++k) {
                        float eu = (float)dx[k] * u.x + (float)dy[k] * u.y;
                        cells_[idx].f[k] = weights[k] * rho * (1.0f + 3.0f * eu + 4.5f * eu * eu - 1.5f * u2);
                    }

                    // Initialize next_cells for boundaries too
                    next_cells_[idx] = cells_[idx];
                }
            }
        }
    }

} // namespace Boidsish
