#pragma once

#include "entity.h"
#include "model.h"
#include "fire_effect.h"

namespace Boidsish {

// Represents the health state of a damageable entity
enum class HealthState {
    ALIVE,
    DYING,
    DEAD
};

// An entity that can take damage and be destroyed
class DamageableEntity : public Entity<Model> {
public:
    DamageableEntity(int id, float shield, float armor, float health);

    // Inflicts damage to the entity, taking into account shield and armor
    void ApplyDamage(const EntityHandler& handler, float damage);

    // Called when the entity takes damage, can be overridden for custom effects
    virtual void OnDamage(const EntityHandler& handler, float damage);

    // Checks if the entity is dead
    bool IsDead() const;

    // Getters for health components
    float GetShield() const;
    float GetArmor() const;
    float GetHealth() const;

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;


protected:
    HealthState _healthState;
    float _shield;
    float _armor;
    float _health;
    float _death_timer;
};

} // namespace Boidsish
