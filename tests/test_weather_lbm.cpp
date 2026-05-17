#include <gtest/gtest.h>
#include "weather_lbm_simulator.h"
#include "terrain_generator.h"
#include <glm/glm.hpp>

using namespace Boidsish;

class MockTerrain : public ITerrainGenerator {
public:
    void Update(const Frustum&, const Camera&) override {}
    const std::vector<std::shared_ptr<Terrain>>& GetVisibleChunks() const override { return chunks_; }
    std::vector<std::shared_ptr<Terrain>> GetVisibleChunksCopy() const override { return chunks_; }
    void SetRenderManager(std::shared_ptr<TerrainRenderManager>) override {}
    std::shared_ptr<TerrainRenderManager> GetRenderManager() const override { return nullptr; }
    float GetMaxHeight() const override { return 100.0f; }
    int GetChunkSize() const override { return 32; }
    void SetWorldScale(float) override {}
    float GetWorldScale() const override { return 1.0f; }
    uint32_t GetVersion() const override { return 1; }
    std::tuple<float, glm::vec3> CalculateTerrainPropertiesAtPoint(float, float) const override { return {10.0f, {0,1,0}}; }
    bool Raycast(const glm::vec3&, const glm::vec3&, float, float&) const override { return false; }
    std::vector<glm::vec3> GetPath(glm::vec2, int, float) const override { return {}; }
    glm::vec3 GetPathData(float, float) const override { return {0,0,0}; }
    float GetBiomeControlValue(float, float) const override { return 0.5f; }
    std::tuple<float, glm::vec3> GetTerrainPropertiesAtPoint(float, float) const override { return {10.0f, {0,1,0}}; }
    bool IsPointBelowTerrain(const glm::vec3&) const override { return false; }
    float GetDistanceAboveTerrain(const glm::vec3&) const override { return 10.0f; }
    bool RaycastCached(const glm::vec3&, const glm::vec3&, float, float&, glm::vec3&) const override { return false; }
    bool IsPositionCached(float, float) const override { return true; }
    void InvalidateChunk(std::pair<int, int>) override {}
    TerrainDeformationManager& GetDeformationManager() override { return def_; }
    const TerrainDeformationManager& GetDeformationManager() const override { return def_; }
    uint32_t AddCrater(const glm::vec3&, float, float, float, float) override { return 0; }
    uint32_t AddFlattenSquare(const glm::vec3&, float, float, float, float) override { return 0; }
    uint32_t AddAkira(const glm::vec3&, float) override { return 0; }
    void InvalidateDeformedChunks(std::optional<uint32_t>) override {}
    void GetBiomeIndicesAndWeights(float, int& low_idx, float& t) const override { low_idx = 0; t = 0.5f; }
    void WaitForAllChunks(const Frustum&, const Camera&) override {}

private:
    std::vector<std::shared_ptr<Terrain>> chunks_;
    TerrainDeformationManager def_;
};

TEST(WeatherLbmTest, BasicSimulation) {
    WeatherLbmSimulator sim(16, 16);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    // Run a few steps
    for (int i = 0; i < 10; ++i) {
        sim.Update(0.016f, i * 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    }

    const auto& out = sim.GetOutput();

    // Check reasonable ranges for physical parameters
    EXPECT_GT(out.temperature, 250.0f);
    EXPECT_LT(out.temperature, 350.0f);

    EXPECT_GE(out.humidity, 0.0f);
    EXPECT_LE(out.humidity, 1.0f);

    EXPECT_GT(out.pressure, 800.0f);
    EXPECT_LT(out.pressure, 1200.0f);

    // Check aerosols
    EXPECT_GT(out.mieScattering, 0.0f);
}

TEST(WeatherLbmTest, ViscosityDamping) {
    WeatherLbmSimulator sim(16, 16);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    // Inject high pressure to create high velocity
    sim.InjectPressure(glm::vec3(0.0f), 2000.0f, 0.5f);

    // Run steps to trigger chaos damping
    for (int i = 0; i < 5; ++i) {
        sim.Update(0.1f, i * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    }

    bool foundDamping = false;
    for (const auto& cell : sim.GetCells()) {
        if (cell.viscosityDamping > 0.0f) {
            foundDamping = true;
            break;
        }
    }
    EXPECT_TRUE(foundDamping);
}

TEST(WeatherLbmTest, Conservation) {
    WeatherLbmSimulator sim(8, 8);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    // Check initial total density (rho = sum(f))
    float initialRho = 0.0f;
    for (const auto& cell : sim.GetCells()) {
        for (int i = 0; i < 9; ++i) initialRho += cell.f[i];
    }

    // Step simulation
    sim.Update(0.016f, 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);

    float finalRho = 0.0f;
    for (const auto& cell : sim.GetCells()) {
        for (int i = 0; i < 9; ++i) finalRho += cell.f[i];
    }

    // Mass should be conserved in closed/periodic system
    // (Note: Our boundary conditions might inject/remove mass,
    // but in the middle of a step it should be close)
    EXPECT_NEAR(initialRho, finalRho, 1.0f);
}

TEST(WeatherLbmTest, AtmosphereDerivations) {
    WeatherLbmSimulator sim(16, 16);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    sim.Update(0.016f, 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    const auto& out = sim.GetOutput();

    // Rayleigh scattering should be positive
    EXPECT_GT(out.rayleighScattering.r, 0.0f);
    EXPECT_GT(out.rayleighScattering.g, 0.0f);
    EXPECT_GT(out.rayleighScattering.b, 0.0f);

    // Mie scattering should be positive
    EXPECT_GT(out.mieScattering, 0.0f);

    // Cloud parameters
    EXPECT_GE(out.cloudCoverage, 0.0f);
    EXPECT_LE(out.cloudCoverage, 1.0f);
}

TEST(WeatherLbmTest, NudgingEffect) {
    WeatherLbmSimulator sim(16, 16);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    float initialTemp = 288.15f;
    float targetTemp = 310.15f; // High temperature target

    // Initialize
    sim.Update(0.016f, 0.0f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, initialTemp, 1013.25f, 0.5f);
    float tempBefore = sim.GetOutput().temperature;

    // Run for many steps with a high target temperature
    for (int i = 0; i < 100; ++i) {
        sim.Update(0.1f, i * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, targetTemp, 1013.25f, 0.5f);
    }

    float tempAfter = sim.GetOutput().temperature;

    // Temperature should have moved towards the target
    EXPECT_GT(tempAfter, tempBefore);
    // Tolerance accounts for noise (2.5K) and relaxation lag
    EXPECT_NEAR(tempAfter, targetTemp, 10.0f);

    // Test pressure nudging
    float targetPressure = 1100.0f;
    for (int i = 0; i < 100; ++i) {
        sim.Update(0.1f, (100 + i) * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, targetTemp, targetPressure, 0.5f);
    }

    float pressureAfter = sim.GetOutput().pressure;
    EXPECT_GT(pressureAfter, 1013.25f);
    // Tolerance accounts for rhoTolerance (0.01 ~ 10hPa) and noise (0.015 ~ 15hPa)
    EXPECT_NEAR(pressureAfter, targetPressure, 30.0f);
}

TEST(WeatherLbmTest, RangeConstraints) {
    WeatherLbmSimulator sim(16, 16);
    MockTerrain terrain;
    glm::vec3 cameraPos(0.0f);

    // 1. Force into a high range
    WeatherLbmSimulator::Constraints constraints;
    constraints.temperature.min = 310.0f;
    constraints.temperature.max = 320.0f;
    sim.SetConstraints(constraints);

    for (int i = 0; i < 100; ++i) {
        sim.Update(0.1f, i * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    }

    float tempHigh = sim.GetOutput().temperature;
    EXPECT_GE(tempHigh, 305.0f); // Pushed up towards 310+

    // 2. Clear constraints and see it trend back to global target (288.15)
    sim.SetConstraints({});
    for (int i = 0; i < 100; ++i) {
        sim.Update(0.1f, (100 + i) * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    }

    float tempBack = sim.GetOutput().temperature;
    EXPECT_LT(tempBack, tempHigh - 2.0f);

    // 3. Set a specific target constraint
    constraints = {};
    constraints.temperature.target = 300.0f;
    sim.SetConstraints(constraints);
    for (int i = 0; i < 100; ++i) {
        sim.Update(0.1f, (200 + i) * 0.1f, 12.0f, terrain, cameraPos, 0.075f, 0.065f, 288.15f, 1013.25f, 0.5f);
    }
    EXPECT_NEAR(sim.GetOutput().temperature, 300.0f, 10.0f);
}
