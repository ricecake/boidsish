#include "graphics.h"
#include "fluid_lbm_manager.h"
#include "model.h"
#include <memory>
#include <iostream>

using namespace Boidsish;

class FluidShape : public Shape {
public:
    FluidShape(FluidLbmManager* manager, Visualizer* viz) : manager_(manager), viz_(viz) {}

    void render() const override {
        manager_->SetTerrainData(viz_->GetTerrainHeightmapTexture(), viz_->GetTerrainChunkGridTexture(), viz_->GetTerrainDataUbo());
        manager_->Render(viz_->GetViewMatrix(), viz_->GetProjectionMatrix(), viz_->GetCamera().pos(), viz_->GetDepthTexture());
    }

    AABB GetAABB() const override {
        auto config = manager_->GetConfig();
        return AABB(config.worldOrigin, config.worldOrigin + config.worldScale);
    }
    std::string GetInstanceKey() const override { return "FluidSimulation"; }
private:
    FluidLbmManager* manager_;
    Visualizer* viz_;
};

int main() {
    try {
        Visualizer viz(1280, 720, "3D LBM Fluid Demo");

        // Initial camera setup
        Camera cam(0.0f, 15.0f, 30.0f, -20.0f, 0.0f);
        viz.SetCamera(cam);

        FluidLbmManager fluidManager;
        FluidLbmConfig config;
        config.resolution = {64, 64, 64};
        config.worldScale = {30.0f, 30.0f, 30.0f};
        config.worldOrigin = {-15.0f, 2.0f, -15.0f};
        config.gravity = 20.0f; // High gravity for quick drop
        config.viscosity = 0.005f; // Low viscosity for splashes
        fluidManager.Initialize(config);

        // Drop a ball of water
        fluidManager.InjectFluid({0.0f, 25.0f, 0.0f}, 5.0f, 0.5f);

        viz.AddUpdateHandler([&](float totalTime, float dt) {
            fluidManager.Step(dt);

            // Periodically inject more fluid for continuous action
            static float lastInject = 0;
            if (totalTime - lastInject > 5.0f) {
                fluidManager.InjectFluid({0.0f, 25.0f, 0.0f}, 4.0f, 0.5f);
                lastInject = totalTime;
            }
        });

        auto fluidShape = std::make_shared<FluidShape>(&fluidManager, &viz);

        viz.AddShapeHandler([&](float /*totalTime*/) {
            std::vector<std::shared_ptr<Shape>> shapes;
            shapes.push_back(fluidShape);
            return shapes;
        });

        viz.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
