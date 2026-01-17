#include "Bullet.h"
#include "PaperPlane.h"
#include "entity.h"

namespace Boidsish {
    Bullet::Bullet(int id, Vector3 pos, Vector3 vel) : Entity<Model>(id, "dot", false) {
        SetPosition(pos);
        SetVelocity(vel);
        shape_->SetScale(glm::vec3(0.1f, 0.1f, 0.5f));
    }

    void Bullet::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
        lifetime_ -= delta_time;
        if (lifetime_ <= 0.0f) {
            handler.QueueRemoveEntity(GetId());
            return;
        }

        auto entities = handler.GetAllEntities();
        for (auto& entity_kv : entities) {
            auto entity = entity_kv.second;
            if (entity.get() == this || dynamic_cast<PaperPlane*>(entity.get()) || dynamic_cast<Bullet*>(entity.get())) {
                continue;
            }

            if (glm::distance(GetPosition().Toglm(), entity->GetPosition().Toglm()) < 2.0f) {
                handler.QueueRemoveEntity(GetId());
                return;
            }
        }
    }
}
