#pragma once

#include "entity.h"
#include "model.h"
#include "PaperPlaneInputController.h"
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace Boidsish {

class PaperPlane : public Entity<Model> {
public:
    PaperPlane(int id = 0);

    void SetController(std::shared_ptr<PaperPlaneInputController> controller);
    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
    void UpdateShape() override;

    void TriggerDamage();
    bool IsDamagePending();
    void AcknowledgeDamage();

    float GetHealth() const;
    float GetShield() const;

private:
    std::shared_ptr<PaperPlaneInputController> controller_;
    glm::quat                                  orientation_;
    glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
    float                                      forward_speed_;
    float                                      time_to_fire = 0.25f;
    bool                                       fire_left = true;
    int                                        damage_pending_ = 0;
    float                                      health = 100.0f;
    float                                      shield = 100.0f;
};

} // namespace Boidsish
