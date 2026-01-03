#include "fire_effect.h"

namespace Boidsish {

FireEffect::FireEffect(const glm::vec3& position, FireEffectStyle style, const glm::vec3& direction, const glm::vec3& velocity)
    : position_(position),
      style_(style),
      direction_(direction),
      velocity_(velocity) {}

} // namespace Boidsish