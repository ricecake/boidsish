#include "DamageableEntity.h"
#include "graphics.h"

namespace Boidsish {

DamageableEntity::DamageableEntity(int id, float shield, float armor, float health)
    : Entity<Model>(id), _healthState(HealthState::ALIVE), _shield(shield), _armor(armor), _health(health), _death_timer(0.0f) {}

void DamageableEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    if (_healthState == HealthState::DYING) {
        _death_timer -= delta_time;
        if (_death_timer <= 0) {
            _healthState = HealthState::DEAD;
            handler.QueueRemoveEntity(id_);
        }
    }
}

void DamageableEntity::ApplyDamage(const EntityHandler& handler, float damage) {
    if (_healthState != HealthState::ALIVE) {
        return;
    }

    float remainingDamage = damage;

    // Deplete shields first
    if (_shield > 0) {
        float shieldDamage = std::min(_shield, remainingDamage);
        _shield -= shieldDamage;
        remainingDamage -= shieldDamage;
    }

    // Apply remaining damage to armor and health
    if (remainingDamage > 0) {
        float armorDamage = remainingDamage * (_armor / 100.0f);
        _health -= (remainingDamage - armorDamage);
    }

    OnDamage(handler, damage);

    if (_health <= 0) {
        _healthState = HealthState::DYING;
        _death_timer = 7.0f; // 5-10 seconds
        SetColor(0.0f, 0.0f, 0.0f, 1.0f); // Turn black

        auto pos = GetPosition();
        handler.EnqueueVisualizerAction([this, pos, &handler]() {
            handler.vis->AddFireEffect(
                glm::vec3(pos.x, pos.y, pos.z),
                FireEffectStyle::Explosion,
                glm::vec3(0, 1, 0),
                glm::vec3(0, 0, 0),
                -1,
                2.0f
            );
            handler.vis->AddFireEffect(
                glm::vec3(pos.x, pos.y, pos.z),
                FireEffectStyle::Fire,
                glm::vec3(0, 1, 0),
                glm::vec3(0, 0, 0),
                -1,
                _death_timer
            );
        });
    }
}

void DamageableEntity::OnDamage(const EntityHandler& handler, float damage) {
    // Can be overridden by subclasses for custom effects
}

bool DamageableEntity::IsDead() const {
    return _healthState == HealthState::DEAD;
}

float DamageableEntity::GetShield() const {
    return _shield;
}

float DamageableEntity::GetArmor() const {
    return _armor;
}

float DamageableEntity::GetHealth() const {
    return _health;
}

} // namespace Boidsish
