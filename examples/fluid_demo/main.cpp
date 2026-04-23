#include "graphics.h"
#include "fluid_lbm_manager.h"
#include "model.h"
#include <memory>
#include <iostream>

using namespace Boidsish;

class FluidShape : public Shape {
public:
    FluidShape(FluidLbmManager* manager) : manager_(manager) {}
    void render() const override {
        // This is called during the main render pass
    }
    AABB GetAABB() const override {
        auto config = manager_->GetConfig();
        return AABB(config.worldOrigin, config.worldOrigin + config.worldScale);
    }
    std::string GetInstanceKey() const override { return "FluidSimulation"; }
private:
    FluidLbmManager* manager_;
};

int main() {
    try {
        Visualizer viz(1280, 720, "3D LBM Fluid Demo");

        FluidLbmManager fluidManager;
        FluidLbmConfig config;
        config.resolution = {64, 64, 64};
        config.worldScale = {20.0f, 20.0f, 20.0f};
        config.worldOrigin = {-10.0f, 0.0f, -10.0f};
        fluidManager.Initialize(config);

        // Drop a ball of water
        fluidManager.InjectFluid({0.0f, 15.0f, 0.0f}, 3.0f, 1.0f);

        viz.AddUpdateHandler([&](float dt, float /*totalTime*/) {
            fluidManager.Step(dt);
        });

        auto fluidShape = std::make_shared<FluidShape>(&fluidManager);

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
