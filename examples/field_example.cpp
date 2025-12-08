#include "field_handler.h"
#include <iostream>

using namespace Boidsish;

class TornadoEmitter : public FieldEmitter {
public:
    TornadoEmitter(const Vector3& center, float strength, float radius)
        : center_(center), strength_(strength), radius_(radius) {}

    Vector3 GetFieldContribution(const Vector3& position) const override {
        Vector3 diff = position - center_;
        float dist = diff.Magnitude();
        if (dist > radius_ || dist == 0) {
            return Vector3::Zero();
        }
        float falloff = 1.0f - (dist / radius_);
        Vector3 tangential_dir(-diff.z, 0, diff.x);
        return tangential_dir.Normalized() * strength_ * falloff;
    }

    AABB GetBoundingBox() const override {
        return AABB{
            center_ - Vector3(radius_, radius_, radius_),
            center_ + Vector3(radius_, radius_, radius_)
        };
    }
private:
    Vector3 center_;
    float strength_;
    float radius_;
};

int main() {
    try {
        Visualizer viz(1024, 768, "Vector Field Example");
        Camera camera(15.0f, 15.0f, 15.0f, -30.0f, -135.0f, 45.0f);
        viz.SetCamera(camera);

        auto handler = std::make_shared<VectorFieldHandler>(30, 30, 30);
        auto emitter = std::make_shared<TornadoEmitter>(Vector3(15, 15, 15), 5.0f, 10.0f);
        handler->AddEmitter(emitter);

        for (int i = 0; i < 50; ++i) {
            handler->AddEntity<FieldEntity>();
            auto entity = handler->GetEntity(i);
            float x = 10 + (rand() % 10);
            float y = 10 + (rand() % 10);
            float z = 10 + (rand() % 10);
            entity->SetPosition(x, y, z);
        }

        viz.SetDotHandler([handler](float time) {
            return (*handler)(time);
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
