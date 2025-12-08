#include "field_handler.h"

namespace Boidsish {

void VectorFieldHandler::PreTimestep(float time, float delta_time) {
    (void)time;
    (void)delta_time;
    VectorField& next_field = *fields_[1 - current_field_];
    next_field.Clear();

    for (const auto& emitter : emitters_) {
        AABB box = emitter->GetBoundingBox();
        int x_start = static_cast<int>(box.min.x);
        int y_start = static_cast<int>(box.min.y);
        int z_start = static_cast<int>(box.min.z);
        int x_end = static_cast<int>(box.max.x);
        int y_end = static_cast<int>(box.max.y);
        int z_end = static_cast<int>(box.max.z);

        for (int z = z_start; z < z_end; ++z) {
            for (int y = y_start; y < y_end; ++y) {
                for (int x = x_start; x < x_end; ++x) {
                    Vector3 position(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    next_field.AddValue(x, y, z, emitter->GetFieldContribution(position));
                }
            }
        }
    }
}

void VectorFieldHandler::PostTimestep(float time, float delta_time) {
    (void)time;
    (void)delta_time;
    const VectorField& field = GetCurrentField();

    for (auto& pair : GetAllEntities()) {
        auto entity = std::dynamic_pointer_cast<FieldEntity>(pair.second);
        if (entity) {
            Vector3 pos = entity->GetPosition();
            int field_x = static_cast<int>(pos.x);
            int field_y = static_cast<int>(pos.y);
            int field_z = static_cast<int>(pos.z);
            Vector3 field_force = field.GetValue(field_x, field_y, field_z);
            entity->SetVelocity(entity->GetVelocity() + field_force * delta_time);
        }
    }

    SwapFields();
}

}