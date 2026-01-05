#include "fire_effect.h"

namespace Boidsish {

	FireEffect::FireEffect(
		const glm::vec3& position,
		FireEffectStyle  style,
		const glm::vec3& direction,
		const glm::vec3& velocity,
		int              max_particles
	):
		position_(position),
		style_(style),
		direction_(direction),
		id_(count++),
		velocity_(velocity),
		max_particles_(max_particles) {}

} // namespace Boidsish