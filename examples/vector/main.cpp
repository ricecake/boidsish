#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <ranges>

#include "dot.h"
#include "fire_effect_manager.h"
#include "graph.h"
#include "graphics.h"
#include "logger.h"
#include "spatial_entity_handler.h"
#include <GLFW/glfw3.h>
#include <RTree.h>

using namespace Boidsish;

class MakeBranchAttractor {
private:
	std::random_device                    rd;
	std::mt19937                          eng;
	std::uniform_real_distribution<float> x;
	std::uniform_real_distribution<float> y;
	std::uniform_real_distribution<float> z;

public:
	MakeBranchAttractor(): eng(rd()), x(-1, 1), y(0, 1), z(-1, 1) {}

	Vector3 operator()(float r) { return r * Vector3(x(eng), y(eng), z(eng)).Normalized(); }
};

static auto fruitPlacer = MakeBranchAttractor();

// A new shape type that has the clone effect enabled.
class CloneableDot: public Dot {
public:
	// Inherit constructors from Dot
	using Dot::Dot;

	std::vector<VisualEffect> GetActiveEffects() const override { return {VisualEffect::FREEZE_FRAME_TRAIL}; }
};

class VectorDemoEntity: public Entity<CloneableDot> {
public:
	VectorDemoEntity(int id, const Vector3& start_pos);
	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
	float hunger_time = 0.0f;
	float energy = 50.0f;
	float phase_;
	int   target_id = -1;
};

class FlockingEntity: public Entity<> {
public:
	FlockingEntity(int id, const Vector3& start_pos);
	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	float GetValue() const { return energy; }

private:
	float   hunger_time = 0.0f;
	float   energy = 50.0f;
	Vector3 CalculateSeparation(
		const std::vector<std::shared_ptr<FlockingEntity>>&   neighbors,
		const std::vector<std::shared_ptr<VectorDemoEntity>>& predators
	);
	Vector3 CalculateAlignment(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
	Vector3 CalculateCohesion(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
};

class FruitEntity: public Entity<> {
public:
	FruitEntity(int id);
	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	float GetValue() const { return value; }

private:
	float phase_;
	float value;
};

FruitEntity::FruitEntity(int id): Entity<>(id), value(0) {
	// Vector3 start_pos((rand() % 10 - 5) * 2.0f, 1 + (rand() % 10), (rand() % 6 - 3) * 2.0f);
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
	v.x *= 0.95f;
	v.z *= 0.95f;
	v.y *= 1.0005f;
	v += Vector3(sin(rand()), sin(rand()), sin(rand())).Normalized() / 4;
	SetVelocity(v);
}

VectorDemoEntity::VectorDemoEntity(int id, const Vector3& start_pos): Entity<CloneableDot>(id), phase_(0.0f) {
	SetPosition(start_pos);
	SetSize(10.0f);
	SetTrailLength(100);
	SetTrailIridescence(false);
}

void VectorDemoEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
	(void)time; // Mark unused parameters
	phase_ += delta_time;

	auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
	auto  current_pos = GetPosition();
	auto  targetInstance = std::static_pointer_cast<FlockingEntity>(handler.GetEntity(target_id));

	if (targetInstance != nullptr) {
		auto target = targetInstance->GetPosition();
		auto to_target = target - current_pos;
		auto distance_to_target = to_target.Magnitude();
		if (distance_to_target <= 0.4f) {
			SetVelocity(3 * to_target);
			SetColor(1.0f, 0, 0, 1.0f);

			hunger_time -= targetInstance->GetValue() / 100 * hunger_time;
			hunger_time = std::max(0.0f, hunger_time);

			handler.QueueRemoveEntity(target_id);
			// Vector3 start_pos((rand() % 10 - 5) * 2.0f, (rand() % 6 - 3) * 2.0f, (rand() % 10 - 5) * 2.0f);
			// handler.QueueAddEntity<FlockingEntity>(start_pos);
			return;
		}
	}

	targetInstance = spatial_handler.FindNearest<FlockingEntity>(current_pos);
	if (!targetInstance) {
		return;
	}

	target_id = targetInstance->GetId();
	auto    target = targetInstance->GetPosition();
	auto    to_target = target - current_pos;
	Vector3 direction = to_target.Normalized();

	Vector3 spread = Vector3(0, 0, 0);
	auto    avoids = spatial_handler.GetEntitiesInRadius<VectorDemoEntity>(current_pos, 1.0f);
	for (auto& a : avoids) {
		if (a.get() == this)
			continue;
		spread += (current_pos - a->GetPosition()).Normalized();
	}

	// Add some orbital motion using cross product
	Vector3 up = Vector3::Up();
	Vector3 tangent = direction.Cross(up).Normalized();

	// Combine linear movement with orbital motion
	Vector3 linear_vel = direction * 2.0f;
	Vector3 orbital_vel = tangent * sin(phase_ * 3.0f) * 1.5f;
	Vector3 total_velocity = linear_vel + orbital_vel + spread;

	SetVelocity(total_velocity);

	hunger_time += delta_time;
	hunger_time = std::min(100.0f, hunger_time);
	if (hunger_time < 5) {
		energy += delta_time;
	} else if (hunger_time > 15) {
		energy -= delta_time;
	}

	if (energy < 10) {
		logger::LOG("DEAD Preadator");

		handler.QueueRemoveEntity(GetId());
	} else if (energy >= 60) {
		logger::LOG("New Preadator");
		energy -= 25;
		handler.QueueAddEntity<VectorDemoEntity>(GetPosition());
	}

	// Color based on velocity magnitude and direction
	Vector3 vel = GetVelocity();
	float   speed = vel.Magnitude();
	Vector3 vel_normalized = vel.Normalized();

	float r = 0.5f + 0.5f * std::abs(vel_normalized.x);
	float g = 0.5f + 0.5f * std::abs(vel_normalized.y);
	float b = 0.5f + 0.3f * (speed / 5.0f); // Blue based on speed
	SetColor(r, g, b, 1.0f);
	SetTrailLength(2 * energy);
}

FlockingEntity::FlockingEntity(int id, const Vector3& start_pos): Entity<>(id) {
	SetPosition(start_pos);
	SetSize(5.0f);
	SetTrailIridescence(true);
	SetTrailLength(25);
	Vector3 startVel((rand() % 30 - 15) * 2.0f, (rand() % 10 - 5) * 2.0f, (rand() % 16 - 8) * 2.0f);

	SetVelocity(startVel);
}

void FlockingEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
	(void)time;
	(void)delta_time; // Mark unused parameters

	auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
	auto  position = GetPosition();

	// Get neighbors and predators using spatial queries
	auto neighbors = spatial_handler.GetEntitiesInRadius<FlockingEntity>(position, 6.0f);
	auto predators = spatial_handler.GetEntitiesInRadius<VectorDemoEntity>(position, 2.0f);

	Vector3 pred;
	for (auto& a : predators) {
		auto pos = a->GetPosition();
		auto dis = position.DistanceTo(pos);
		auto dir = (position - pos).Normalized();
		pred += dir + (1 / (dis)*pos.Cross(GetPosition()).Normalized());
	}
	pred.Normalize();

	auto targetInstance = spatial_handler.FindNearest<FruitEntity>(position);
	if (!targetInstance) {
		return;
	}

	auto food = targetInstance->GetPosition();
	auto foodDistance = position.DistanceTo(food);

	if (foodDistance <= 0.6f) {
		SetVelocity(3 * (food - position));
		SetColor(1.0f, 0, 0, 1.0f);
		hunger_time -= targetInstance->GetValue() / 100 * hunger_time;
		hunger_time = std::max(0.0f, hunger_time);

		handler.QueueRemoveEntity(targetInstance->GetId());
		// handler.QueueAddEntity<FruitEntity>();
		return;
	}

	auto distance = (foodDistance / 4 + hunger_time / 15 * (1 / std::min(1.0f, foodDistance / 5))) *
		(food - position).Normalized();

	Vector3 separation = CalculateSeparation(neighbors, predators);
	Vector3 alignment = CalculateAlignment(neighbors);
	Vector3 cohesion = CalculateCohesion(neighbors);
	Vector3 total_force = separation * 2.0f + alignment * 0.50f + cohesion * 1.30f + distance * 1.0f + pred * 2.0f;

	auto newVel = (GetVelocity() + total_force.Normalized()).Normalized();
	SetVelocity(newVel * 3);

	hunger_time += delta_time;
	hunger_time = std::min(100.0f, hunger_time);
	if (hunger_time < 5) {
		energy += delta_time;
	} else if (hunger_time > 15) {
		energy -= delta_time;
	}

	if (energy < 10) {
		logger::LOG("DEAD Flocker");
		handler.QueueRemoveEntity(GetId());
	} else if (energy >= 60) {
		energy -= 25;
		logger::LOG("New Flocker");
		handler.QueueAddEntity<FlockingEntity>(GetPosition());
	}

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
	SetTrailLength(energy);
}

Vector3 FlockingEntity::CalculateSeparation(
	const std::vector<std::shared_ptr<FlockingEntity>>&   neighbors,
	const std::vector<std::shared_ptr<VectorDemoEntity>>& predators
) {
	Vector3 separation = Vector3::Zero();
	Vector3 my_pos = GetPosition();
	int     count = 0;
	float   separation_radius = 2.50f;

	auto total_distance = 0.0f;
	for (auto& p : predators) {
		auto dist = p->GetPosition().DistanceTo(my_pos);
		if (dist <= 2) {
			total_distance += 1 / (dist * dist);
		}
	}
	separation_radius *= std::max(1.0f, total_distance);

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
	float   alignment_radius = 3.50f;

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
	float   cohesion_radius = 6.0f;

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
class VectorDemoHandler: public SpatialEntityHandler {
	std::random_device rd;
	std::mt19937       eng;

public:
	VectorDemoHandler(task_thread_pool::task_thread_pool& thread_pool): SpatialEntityHandler(thread_pool), eng(rd()) {
		std::cout << "=== Vector3 Operations Demo ===" << std::endl;

		// Create some vector demo entities
		for (int i = 0; i < 4; i++) {
			Vector3 start_pos(10 * sin(i / 4), 1.0f, 10 * cos(i / 6.0f));
			AddEntity<VectorDemoEntity>(start_pos);
		}

		// Create a flock of entities
		for (int i = 0; i < 32; i++) {
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, -1, (rand() % 10 - 5) * 2.0f);
			AddEntity<FlockingEntity>(start_pos);
		}

		for (int i = 0; i < 8; i++) {
			AddEntity<FruitEntity>();
		}

		std::cout << "Created 8 flocking entities and 5 target-seeking entities" << std::endl;
		std::cout << "Demonstrating Vector3 operations: addition, subtraction, normalization," << std::endl;
		std::cout << "dot product, cross product, magnitude, and distance calculations!" << std::endl;
		std::cout << "Flocking entities now automatically discover each other through the handler!" << std::endl;
	}

	void PreTimestep(float time, float delta_time) {
		// logger::LOG("Time delta", delta_time);
		float fruitRate = 2.0f;
		auto  numFlocker = GetEntitiesByType<FlockingEntity>().size();
		if (numFlocker <= 4) {
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, (rand() % 6 - 3) * 2.0f, (rand() % 10 - 5) * 2.0f);
			AddEntity<FlockingEntity>(start_pos);
			fruitRate++;
		} else if (numFlocker > 96) {
			fruitRate--;
		}

		fruitRate = std::max(4.0f, fruitRate);

		if (GetEntitiesByType<VectorDemoEntity>().size() < 1) {
			Vector3 start_pos(10 * sin(rand() / 4), 1.0f, 10 * cos(rand() / 6.0f));
			AddEntity<VectorDemoEntity>(start_pos);
		}

		float weightedOdds = delta_time * fruitRate *
			std::clamp(1.0f - (GetEntitiesByType<FruitEntity>().size() / 32), 0.0f, 1.0f);
		std::bernoulli_distribution dist(weightedOdds);
		bool                        makeFruit = dist(eng);
		if (makeFruit) {
			AddEntity<FruitEntity>();
		}
	}
};

std::vector<std::shared_ptr<Shape>> GraphExample(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;
	auto                                graph = std::make_shared<Graph>(0, 0, 0, 0);

	// // Add vertices in a chain
	// graph->vertices.push_back({Vector3(-4, 0, 0), 10.0f, 1, 0, 0, 1});
	// graph->vertices.push_back({Vector3(-2, 2, 1.0f * sin(time)), 12.0f, 0, 1, 0, 1});
	// graph->vertices.push_back({Vector3(0, 0, 0), 15.0f + 20.0f * sin(time), 0, 0, 1, 1});
	// graph->vertices.push_back({Vector3(2, 2, -1.0f * sin(time)), 12.0f, 1, 1, 0, 1});
	// graph->vertices.push_back({Vector3(4, 0, 0), 10.0f, 1, 0, 1, 1});

	// // Add edges to connect the vertices in a chain
	// graph->edges.push_back({0, 1}); // from_vertex_index, to_vertex_index
	// graph->edges.push_back({1, 2});
	// graph->edges.push_back({2, 3});
	// graph->edges.push_back({3, 4});

	auto root = graph->AddVertex(Vector3(0, 0, 0), 48.0f, 0, 0, 1, 1);
	auto trunk = graph->AddVertex(Vector3(0, 6, 0), 16.0f, 0, 1, 1, 1);
	root.Link(trunk);

	graph
		->AddVertex(
			Vector3(0, 11, 0),
			24.0f,
			abs(sin(time / 2)),
			abs(sin(time / 3 + M_PI / 3)),
			abs(sin(time / 5 + (2 * M_PI / 3))),
			1
		)
		.Link(trunk);
	graph
		->AddVertex(
			Vector3(3, 10 + sin(time), cos(time)),
			24.0f,
			abs(sin(time / 2)),
			abs(sin(time / 5 + (2 * M_PI / 3))),
			abs(sin(time / 3 + M_PI / 3)),
			1
		)
		.Link(trunk);
	graph
		->AddVertex(
			Vector3(-3, 10 + sin(time), cos(time)),
			24.0f,
			abs(sin(time / 3 + (2 * M_PI / 3))),
			abs(sin(time / 2 + M_PI / 3)),
			abs(sin(time / 5)),
			1
		)
		.Link(trunk);
	graph
		->AddVertex(
			Vector3(cos(time), 10 + sin(time), 3),
			24.0f,
			abs(sin(time / 3 + M_PI / 3)),
			abs(sin(time / 5)),
			abs(sin(time / 2 + (2 * M_PI / 3))),
			1
		)
		.Link(trunk);
	graph
		->AddVertex(
			Vector3(cos(time), 10 + sin(time), -3),
			24.0f,
			abs(sin(time / 5 + (2 * M_PI / 3))),
			abs(sin(time / 3 + M_PI / 3)),
			abs(sin(time / 2)),
			1
		)
		.Link(trunk);

	shapes.push_back(graph);
	return shapes;
}

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
		VectorDemoHandler handler(viz.GetThreadPool());
		viz.AddShapeHandler(std::ref(handler));
		viz.AddShapeHandler(std::ref(GraphExample));

		std::cout << "Vector Demo Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera" << std::endl;
		std::cout << "  Space/Shift - Move up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  0 - Toggle auto-camera" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;

		viz.AddInputCallback([&](const InputState& state) {
			if (state.key_down[GLFW_KEY_G]) {
				viz.TogglePostProcessingEffect("Film Grain");
			}
		});

		// Main loop
		auto fire_manager = viz.GetFireEffectManager();
		auto emitter1 = fire_manager->AddEffect(
			glm::vec3(0, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			5000
		);
		auto emitter2 = fire_manager->AddEffect(
			glm::vec3(5, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			10000
		);
		auto emitter3 = fire_manager->AddEffect(
			glm::vec3(-5, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			25
		);

		float start_time = 0.0f;
		bool  emitter_removed = false;
		while (!viz.ShouldClose()) {
			viz.Update();
			viz.Render();
			start_time += viz.GetLastFrameTime();
			if (start_time > 5.0f && !emitter_removed) {
				fire_manager->RemoveEffect(emitter2);
				emitter_removed = true;
			}
		}

		std::cout << "Vector demo ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
