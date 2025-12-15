#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "shape_handler.h"

class DemoHandler : public Boidsish::ShapeHandler {
public:
    const std::vector<std::shared_ptr<Boidsish::Shape>>& GetShapes(float t) override {
        if (shapes_.empty()) {
            for (int i = 0; i < 5; ++i) {
                auto dot = std::make_shared<Boidsish::Dot>();
                dot->SetPosition(i * 2.0f - 4.0f, sin(t + i), 0.0f);
                dot->SetColor(1.0f, 0.0f, 0.0f);
                shapes_.push_back(dot);
            }
            // A specific dot opts into the ripple effect
            shapes_[2]->GetEffectSet().SetEffectState(Boidsish::VisualEffect::RIPPLE, Boidsish::EffectState::ENABLED);
        }

        for (size_t i = 0; i < shapes_.size(); ++i) {
            shapes_[i]->SetPosition(i * 2.0f - 4.0f, sin(t + i), 0.0f);
        }

        return shapes_;
    }

private:
    std::vector<std::shared_ptr<Boidsish::Shape>> shapes_;
};

int main() {
    Boidsish::Visualizer visualizer(1280, 720, "Artistic Effects Demo");

    auto handler = std::make_shared<DemoHandler>();
    // The handler enables color shift for all its dots
    handler->GetEffectSet().SetEffectState(Boidsish::VisualEffect::COLOR_SHIFT, Boidsish::EffectState::ENABLED);

    visualizer.AddShapeHandler(handler);
    visualizer.Run();
    return 0;
}