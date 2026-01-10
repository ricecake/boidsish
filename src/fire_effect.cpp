#include "fire_effect.h"

namespace Boidsish {

	FireEffect::FireEffect(
		const glm::vec3& position,
		FireEffectStyle  style,
		const glm::vec3& direction,
		const glm::vec3& velocity,
		int              max_particles,
		float            lifetime,
		bool             needs_terrain_data
	):
		position_(position),
		style_(style),
		direction_(direction),
		id_(count++),
		velocity_(velocity),
		max_particles_(max_particles),
		lifetime_(lifetime),
		needs_terrain_data_(needs_terrain_data) {}

} // namespace Boidsish
