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
        sim.Update(0.016f, i * 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f);
    }

    const auto& out = sim.GetOutput();

    // Check reasonable ranges for physical parameters
    EXPECT_GT(out.temperature, 250.0f);
    EXPECT_LT(out.temperature, 350.0f);

    EXPECT_GE(out.humidity, 0.0f);
    EXPECT_LE(out.humidity, 1.0f);

    EXPECT_GT(out.pressure, 800.0f);
    EXPECT_LT(out.pressure, 1200.0f);
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
    sim.Update(0.016f, 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f);

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

    sim.Update(0.016f, 0.016f, 12.0f, terrain, cameraPos, 0.075f, 0.065f);
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
