/**
 * @file main.cpp
 * @brief Delaunay Blob Demo - Boid-driven control points forming a dynamic mesh
 *
 * This example creates a DelaunayBlob shape driven by boid entities.
 * Each boid controls a vertex of the blob, and as they move with flocking
 * behavior, the blob morphs and deforms dynamically.
 */

#include <cmath>
#include <memory>
#include <random>
#include <vector>

#include "delaunay_blob.h"
#include "entity.h"
#include "graphics.h"

using namespace Boidsish;

// === Blob Control Point Entity ===
// An entity that drives a single control point in the DelaunayBlob

class BlobBoid: public Entity<Dot> {
public:
	BlobBoid(int id, std::shared_ptr<DelaunayBlob> blob, int point_id):
		Entity<Dot>(id), blob_(blob), point_id_(point_id) {
		// Small visualization dot at the control point
		SetSize(0.5f);
		SetColor(1.0f, 0.3f, 0.1f);
		SetTrailLength(0); // No trails for cleaner look
		                   // SetHidden(true);    // Hide the dots, just show the blob
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		if (delta_time <= 0.0f || delta_time > 1.0f)
			return;

		// Get all other blob boids for flocking
		const auto& entities = handler.GetAllEntities();

		glm::vec3 my_pos = GetPosition();
		glm::vec3 my_vel = GetVelocity();

		// Flocking parameters
		const float perception_radius = 8.0f;
		const float separation_radius = 2.0f;
		const float cohesion_strength = 0.8f;
		const float alignment_strength = 0.5f;
		const float separation_strength = 2.5f;
		const float center_pull_strength = 1.3f;
		const float max_speed = 6.0f;
		const float max_force = 3.0f;

		glm::vec3 cohesion_sum(0.0f);
		glm::vec3 alignment_sum(0.0f);
		glm::vec3 separation_sum(0.0f);
		glm::vec3 waypoint_sum(0.0f);
		int       cohesion_count = 0;
		int       separation_count = 0;

		for (const auto& [other_id, other_entity] : entities) {
			if (other_id == GetId())
				continue;

			auto* other_boid = dynamic_cast<BlobBoid*>(other_entity.get());
			if (!other_boid)
				continue;

			glm::vec3 other_pos = other_boid->GetPosition();
			float     dist = glm::distance(my_pos, other_pos);

			if (dist < perception_radius && dist > 0.001f) {
				// Cohesion: steer towards center of neighbors
				cohesion_sum += other_pos;

				// Alignment: match velocity of neighbors
				alignment_sum += other_boid->GetVelocity().Toglm();

				cohesion_count++;

				// Separation: avoid crowding
				if (dist < separation_radius) {
					glm::vec3 diff = my_pos - other_pos;
					diff /= (dist * dist); // Weight by distance
					separation_sum += diff;
					separation_count++;
				}
			}
		}

		glm::vec3 steering(0.0f);

		// Apply cohesion
		if (cohesion_count > 0) {
			glm::vec3 center = cohesion_sum / static_cast<float>(cohesion_count);
			glm::vec3 desired = center - my_pos;
			if (glm::length(desired) > 0.001f) {
				desired = glm::normalize(desired) * max_speed;
				glm::vec3 cohesion_force = desired - my_vel;
				steering += cohesion_force * cohesion_strength;
			}
		}

		// Apply alignment
		if (cohesion_count > 0) {
			glm::vec3 avg_vel = alignment_sum / static_cast<float>(cohesion_count);
			if (glm::length(avg_vel) > 0.001f) {
				glm::vec3 desired = glm::normalize(avg_vel) * max_speed;
				glm::vec3 alignment_force = desired - my_vel;
				steering += alignment_force * alignment_strength;
			}
		}

		// Apply separation
		if (separation_count > 0) {
			glm::vec3 avg_sep = separation_sum / static_cast<float>(separation_count);
			if (glm::length(avg_sep) > 0.001f) {
				glm::vec3 desired = glm::normalize(avg_sep) * max_speed;
				glm::vec3 separation_force = desired - my_vel;
				steering += separation_force * separation_strength;
			}
		}

		// Pull towards center to keep blob cohesive
		glm::vec3 to_center = blob_center_ - my_pos;
		float     dist_to_center = glm::length(to_center);
		if (dist_to_center > max_radius_) {
			glm::vec3 center_force = glm::normalize(to_center) * (dist_to_center - max_radius_) * center_pull_strength;
			steering += center_force;
		}

		// Add some organic movement (subtle noise)
		float noise_x = std::sin(time * 0.7f + static_cast<float>(GetId()) * 0.3f) * 0.5f;
		float noise_y = std::cos(time * 0.5f + static_cast<float>(GetId()) * 0.5f) * 0.3f;
		float noise_z = std::sin(time * 0.6f + static_cast<float>(GetId()) * 0.7f) * 0.5f;
		steering += glm::vec3(noise_x, noise_y, noise_z);

		// Limit steering force
		if (glm::length(steering) > max_force) {
			steering = glm::normalize(steering) * max_force;
		}

		// Update velocity
		glm::vec3 new_vel = my_vel + steering * delta_time;

		// Limit speed
		if (glm::length(new_vel) > max_speed) {
			new_vel = glm::normalize(new_vel) * max_speed;
		}

		// Update position
		glm::vec3 new_pos = my_pos + new_vel * delta_time;

		SetPosition(new_pos);
		SetVelocity(new_vel);

		// Update the blob control point
		if (blob_) {
			blob_->SetPointState(point_id_, new_pos, new_vel);

			// Update point color based on velocity (optional visual effect)
			float     speed = glm::length(new_vel) / max_speed;
			glm::vec4 color = glm::mix(
				glm::vec4(0.2f, 0.4f, 0.8f, 0.8f), // Slow: blue
				glm::vec4(1.0f, 0.4f, 0.1f, 0.9f), // Fast: orange
				speed
			);
			blob_->SetPointColor(point_id_, color);
		}

		blob_center_ = glm::vec3(10 * sin(time * 0.1), 15, 15 * cos(time * 0.1));

		UpdateShape();
	}

	void SetBlobCenter(const glm::vec3& center) { blob_center_ = center; }

	void SetMaxRadius(float radius) { max_radius_ = radius; }

	int GetPointId() const { return point_id_; }

private:
	std::shared_ptr<DelaunayBlob> blob_;
	int                           point_id_;
	glm::vec3                     blob_center_{0.0f, 5.0f, 0.0f};
	float                         max_radius_ = 10.0f;
};

int main() {
	// Create visualizer
	// Visualizer visualizer(1280, 720, "Delaunay Blob Demo");//, false, false, false, true);
	auto visualizer = std::make_shared<Visualizer>(1280, 720, "Delaunay Blob Demo");

	// Set up camera
	auto& camera = visualizer->GetCamera();
	camera.x = 0.0f;
	camera.y = 10.0f;
	camera.z = 30.0f;
	camera.pitch = -15.0f;
	camera.yaw = -90.0f;

	// Create the blob shape
	auto blob = std::make_shared<DelaunayBlob>(0);
	blob->SetColor(0.3f, 0.5f, 0.9f);
	blob->SetAlpha(0.85f);
	blob->SetRenderMode(DelaunayBlob::RenderMode::SolidWithWire);
	blob->SetWireframeColor(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
	blob->SetSmoothNormals(true);

	// Create entity handler
	// EntityHandler handler;
	EntityHandler handler(visualizer->GetThreadPool(), visualizer);

	// Random generator for initial positions
	std::random_device                    rd;
	std::mt19937                          gen(rd());
	std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
	std::uniform_real_distribution<float> height_dist(3.0f, 7.0f);

	// Create blob control points and boid entities
	const int num_points = 200;
	glm::vec3 blob_center(0.0f, 5.0f, 0.0f);

	for (int i = 0; i < num_points; ++i) {
		// Random starting position around center
		glm::vec3 pos(blob_center.x + dist(gen), blob_center.y + height_dist(gen) - 5.0f, blob_center.z + dist(gen));

		// Add point to blob and get its ID
		int point_id = blob->AddPoint(pos);

		// Create boid entity that controls this point
		auto boid = std::make_shared<BlobBoid>(i + 1, blob, point_id);
		boid->SetPosition(pos);
		boid->SetBlobCenter(blob_center);
		boid->SetMaxRadius(22.0f);

		// Random initial velocity
		boid->SetVelocity(glm::vec3(dist(gen) * 5.0f, 0.0f, dist(gen) * 0.5f));

		handler.AddEntity(boid->GetId(), boid);
	}

	// Initial tetrahedralization
	blob->Retetrahedralize();

	// Add blob as a persistent shape
	visualizer->AddShape(blob);

	// Add entity update function
	visualizer->AddShapeHandler([&](float time) { return handler(time); });

	// Add lighting
	Light sun;
	sun.type = DIRECTIONAL_LIGHT;
	sun.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
	sun.color = glm::vec3(1.0f, 0.95f, 0.9f);
	sun.intensity = 1.2f;
	visualizer->GetLightManager().AddLight(sun);

	Light fill;
	fill.type = POINT_LIGHT;
	fill.position = glm::vec3(10.0f, 15.0f, 10.0f);
	fill.color = glm::vec3(0.4f, 0.5f, 0.7f);
	fill.intensity = 0.5f;
	visualizer->GetLightManager().AddLight(fill);

	// Run
	visualizer->Run();

	return 0;
}
