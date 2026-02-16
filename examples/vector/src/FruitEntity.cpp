#include "FruitEntity.h"

#include "MakeBranchAttractor.h"

namespace Boidsish {

	// This is a global, but it's only used in this one file.
	// If it were used more broadly, it would be better to pass it as a dependency.
	static MakeBranchAttractor fruitPlacer;

	FruitEntity::FruitEntity(): Entity<>(), value(0) {
		auto start_pos = fruitPlacer(6);
		start_pos.y += 8;
		SetPosition(start_pos);
		SetTrailLength(0);
		SetColor(1, 0.36f, 1);
		SetVelocity(Vector3(0, 1, 0));
		phase_ = start_pos.Magnitude();
	}

	void FruitEntity::UpdateEntity(const EntityHandler& handler, float, float delta_time) {
		phase_ += delta_time;

		auto value_modifier = (sin((4 * phase_) / 8) + 1) / 2;
		value = value_modifier * 100;
		SetSize(4 + 12 * value_modifier);

		if (value < 0) {
			handler.QueueAddEntity<FruitEntity>();
			handler.QueueRemoveEntity(GetId());
		}
		auto v = GetVelocity();
		auto p = GetPosition();
		v.x *= 0.95f;
		v.z *= 0.95f;
		v.y *= 1.0005f;
		if (p.y > 12) {
			v.y /= 2;
		} else if (p.y <= 0) {
			p.y = 0.1f;
			v.y = 0;
		}
		v += Vector3(sin(rand()), sin(rand()), sin(rand())).Normalized() / 4;
		SetVelocity(v);
	}

} // namespace Boidsish
