#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <ranges>

#include "boidsish.h"
#include "logger.h"
#include <RTree.h>

using namespace Boidsish;

class VectorDemoEntity: public Entity {
public:
	VectorDemoEntity(int id, const Vector3& start_pos);
	void UpdateEntity(EntityHandler& handler, float time, float delta_time) override;

private:
	float phase_;
	int   target_id = -1;
};

class FlockingEntity: public Entity {
public:
	FlockingEntity(int id, const Vector3& start_pos);
	void UpdateEntity(EntityHandler& handler, float time, float delta_time) override;

private:
	float hunger_time = 100.0f;
	Vector3 CalculateSeparation(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
	Vector3 CalculateAlignment(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
	Vector3 CalculateCohesion(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
};

class FruitEntity: public Entity {
public:
	FruitEntity(int id);
	void UpdateEntity(EntityHandler& handler, float time, float delta_time) override;
	float GetValue() const {
		return value;
	}
private:
	float value;
};

FruitEntity::FruitEntity(int id): Entity(id), value(rand() % 30) {
	Vector3 start_pos((rand() % 10 - 5) * 2.0f, 1+(rand() % 10), (rand() % 6 - 3) * 2.0f);
	SetPosition(start_pos);
	SetColor(255, 165, 0);
}

void FruitEntity::UpdateEntity(EntityHandler& handler, float time, float delta_time) {
	value -= delta_time;
	if (value <= 0) {
		handler.AddEntity<FruitEntity>();
		handler.RemoveEntity(GetId());
	}
}

VectorDemoEntity::VectorDemoEntity(int id, const Vector3& start_pos): Entity(id), phase_(0.0f) {
	SetPosition(start_pos);
	SetSize(8.0f);
	SetTrailLength(100);
}

void VectorDemoEntity::UpdateEntity(EntityHandler& handler, float time, float delta_time) {
	(void)handler;
	(void)time; // Mark unused parameters
	phase_ += delta_time;

	auto current_pos = GetPosition();
	auto targetInstance = std::static_pointer_cast<FlockingEntity>(handler.GetEntity(target_id));

	if (targetInstance != nullptr) {
		auto target = targetInstance->GetPosition();
		auto to_target = target - current_pos;
		auto distance_to_target = to_target.Magnitude();
		if (distance_to_target <= 0.4f) {
			SetVelocity(3 * to_target);
			SetColor(1.0f, 0, 0, 1.0f);

			handler.RemoveEntity(target_id);
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, (rand() % 6 - 3) * 2.0f, (rand() % 10 - 5) * 2.0f);
			handler.AddEntity<FlockingEntity>(start_pos);
			return;
		}
	}

	auto prey = handler.GetEntitiesByType<FlockingEntity>();
	targetInstance = std::ranges::min(prey, std::ranges::less{}, [&](auto i) {
		return current_pos.DistanceTo(i->GetPosition());
	});
	target_id = targetInstance->GetId();
	auto target = targetInstance->GetPosition();
	auto to_target = target - current_pos;

	// Demonstrate various Vector3 operations
	// Normalize direction and move toward target
	Vector3 direction = to_target.Normalized();

	// Add some orbital motion using cross product
	Vector3 up = Vector3::Up();
	Vector3 tangent = direction.Cross(up).Normalized();

	// Combine linear movement with orbital motion
	Vector3 linear_vel = direction * 2.0f;
	Vector3 orbital_vel = tangent * sin(phase_ * 3.0f) * 1.5f;
	Vector3 total_velocity = linear_vel + orbital_vel;

	SetVelocity(total_velocity);

	// Color based on velocity magnitude and direction
	Vector3 vel = GetVelocity();
	float   speed = vel.Magnitude();
	Vector3 vel_normalized = vel.Normalized();

	float r = 0.5f + 0.5f * std::abs(vel_normalized.x);
	float g = 0.5f + 0.5f * std::abs(vel_normalized.y);
	float b = 0.5f + 0.3f * (speed / 5.0f); // Blue based on speed
	SetColor(r, g, b, 1.0f);
}

FlockingEntity::FlockingEntity(int id, const Vector3& start_pos): Entity(id) {
	SetPosition(start_pos);
	SetSize(5.0f);
	SetTrailLength(150);
	Vector3 startVel((rand() % 30 - 15) * 2.0f, (rand() % 10 - 5) * 2.0f, (rand() % 16 - 8) * 2.0f);

	SetVelocity(startVel);
}

void FlockingEntity::UpdateEntity(EntityHandler& handler, float time, float delta_time) {
	(void)time;
	(void)delta_time; // Mark unused parameters

	// Get all flocking entities from the handler
	auto neighbors = handler.GetEntitiesByType<FlockingEntity>();

	Vector3 separation = CalculateSeparation(neighbors);
	Vector3 alignment = CalculateAlignment(neighbors);
	Vector3 cohesion = CalculateCohesion(neighbors);

	auto position = GetPosition();

	Vector3 pred;
	auto    avoids = handler.GetEntitiesByType<VectorDemoEntity>();
	for (auto& a : avoids) {
		auto pos = a->GetPosition();
		auto dis = position.DistanceTo(pos);
		auto dir = (position - pos).Normalized();
		// pred += dir + (1 / dis * a->GetVelocity().Cross(GetVelocity()).Normalized());
		// pred += dir + (1 / dis * pos.Cross(GetPosition()).Normalized()  );
		pred += dir + (1 / (dis) * pos.Cross(GetPosition()).Normalized());
	}

	pred.Normalize();

	auto foods = handler.GetEntitiesByType<FruitEntity>();
	auto targetInstance = std::ranges::min(foods, std::ranges::less{}, [&](auto i) {
		return position.DistanceTo(i->GetPosition());
	});
	auto food = targetInstance->GetPosition();
	auto foodDistance = position.DistanceTo(food);

	if (foodDistance <= 0.6f) {
		SetVelocity(3 * (food-position));
		SetColor(1.0f, 0, 0, 1.0f);
		hunger_time -= targetInstance->GetValue()/100 * hunger_time;

		handler.RemoveEntity(targetInstance->GetId());
		handler.AddEntity<FruitEntity>();
		return;
	}

	auto distance = hunger_time/15*(1/std::min(1.0f, foodDistance/5))*(food-position).Normalized();

	// Weight the flocking behaviors
	Vector3 total_force = separation * 2.0f + alignment * 0.750f + cohesion * 1.30f + distance * 1.0f + pred * 2.0f;

	auto newVel = (GetVelocity()+total_force.Normalized()).Normalized();
	SetVelocity(newVel*3);

	hunger_time += delta_time;

	// Color based on dominant behavior
	float sep_mag = separation.Magnitude();
	float align_mag = alignment.Magnitude();
	float coh_mag = cohesion.Magnitude();
	float dis_mag = distance.Magnitude();
	float pre_mag = pred.Magnitude();

	float b = (sep_mag + align_mag + coh_mag) / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
	float g = dis_mag / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
	float r = pre_mag / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
	SetColor(r, g, b, 1.0f);
}

Vector3 FlockingEntity::CalculateSeparation(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors) {
	Vector3 separation = Vector3::Zero();
	int     count = 0;
	float   separation_radius = 2.50f;

	Vector3 my_pos = GetPosition();
	for (auto neighbor : neighbors) {
		if (neighbor.get() != this) {
			Vector3 neighbor_pos = neighbor->GetPosition();
			float   distance = my_pos.DistanceTo(neighbor_pos);

			if (distance < separation_radius && distance > 0) {
				Vector3 away = (my_pos - neighbor_pos).Normalized() / distance;
				separation += away;
				count++;
			}
		}
	}

	if (count > 0) {
		separation /= count;
	}
	return separation;
}

Vector3 FlockingEntity::CalculateAlignment(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors) {
	Vector3 average_velocity = Vector3::Zero();
	int     count = 0;
	float   alignment_radius = 3.0f;

	Vector3 my_pos = GetPosition();
	for (auto neighbor : neighbors) {
		if (neighbor.get() != this) {
			Vector3 neighbor_pos = neighbor->GetPosition();
			float   distance = my_pos.DistanceTo(neighbor_pos);

			if (distance < alignment_radius) {
				average_velocity += neighbor->GetVelocity();
				count++;
			}
		}
	}

	if (count > 0) {
		average_velocity /= count;
		return average_velocity.Normalized();
	}
	return Vector3::Zero();
}

Vector3 FlockingEntity::CalculateCohesion(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors) {
	Vector3 center_of_mass = Vector3::Zero();
	int     count = 0;
	float   cohesion_radius = 4.0f;

	Vector3 my_pos = GetPosition();
	for (auto neighbor : neighbors) {
		if (neighbor.get() != this) {
			Vector3 neighbor_pos = neighbor->GetPosition();
			float   distance = my_pos.DistanceTo(neighbor_pos);

			if (distance < cohesion_radius) {
				center_of_mass += neighbor_pos;
				count++;
			}
		}
	}

	if (count > 0) {
		center_of_mass /= count;
		return (center_of_mass - my_pos).Normalized() * 0.5f;
	}
	return Vector3::Zero();
}

// Handler for vector demonstration
class VectorDemoHandler: public EntityHandler {
public:
	VectorDemoHandler() {
		std::cout << "=== Vector3 Operations Demo ===" << std::endl;

		// Create some vector demo entities
		for (int i = 0; i < 5; i++) {
			Vector3 start_pos((i - 2) * 3.0f, 0.0f, 0.0f);
			AddEntity<VectorDemoEntity>(start_pos);
		}

		// Create a flock of entities
		for (int i = 0; i < 48; i++) {
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, (rand() % 6 - 3) * 2.0f, (rand() % 10 - 5) * 2.0f);
			AddEntity<FlockingEntity>(start_pos);
		}

		for (int i = 0; i < 11; i++) {
			AddEntity<FruitEntity>();
		}

		std::cout << "Created 8 flocking entities and 5 target-seeking entities" << std::endl;
		std::cout << "Demonstrating Vector3 operations: addition, subtraction, normalization," << std::endl;
		std::cout << "dot product, cross product, magnitude, and distance calculations!" << std::endl;
		std::cout << "Flocking entities now automatically discover each other through the handler!" << std::endl;
	}
};

int main() {
	try {
		Boidsish::Visualizer viz(1200, 800, "Vector3 Operations Demo");

		// Set up camera
		Camera camera;
		camera.x = 0.0f;
		camera.y = 5.0f;
		camera.z = 15.0f;
		camera.yaw = 0.0f;
		camera.pitch = -15.0f;
		viz.SetCamera(camera);

		// Create and set the vector demo handler
		VectorDemoHandler handler;
		viz.SetDotHandler(std::ref(handler));

		std::cout << "Vector Demo Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera" << std::endl;
		std::cout << "  Space/Shift - Move up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  0 - Toggle auto-camera" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;

		// Main loop
		while (!viz.ShouldClose()) {
			viz.Update();
			viz.Render();
		}

		std::cout << "Vector demo ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}