#include <iostream>
#include <memory>
#include <vector>

#include "entity.h"
#include "graphics.h"
#include "model.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>

using namespace Boidsish;

// Simple entity using a Model
class ModelEntity: public Entity<Model> {
public:
	ModelEntity(int id, const std::string& model_path, const glm::vec3& position, float scale = 1.0f):
		Entity<Model>(id, model_path) {
		SetPosition(position.x, position.y, position.z);
		SetSize(scale); // For ModelEntity, size is used as a base scale
		rigid_body_.linear_friction_ = 0.5f;
		rigid_body_.angular_friction_ = 0.5f;
		rigid_body_.mass_ = 1.0f;
		rigid_body_.inertia_ = glm::vec3(1.0f);
		UpdateShape();
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		// Nothing special to do here, RigidBody handles the physics
	}
};

// Simple entity using a Dot (Sphere)
class DotEntity: public Entity<Dot> {
public:
	DotEntity(int id, const glm::vec3& position, float size = 50.0f): Entity<Dot>(id) {
		SetPosition(position.x, position.y, position.z);
		SetSize(size);
		SetColor(1.0f, 0.5f, 0.2f);
		rigid_body_.linear_friction_ = 0.5f;
		rigid_body_.angular_friction_ = 0.5f;
		rigid_body_.mass_ = 1.0f;
		rigid_body_.inertia_ = glm::vec3(1.0f);
		UpdateShape();
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		// Nothing special to do here
	}
};

class TorqueDemoHandler: public EntityHandler {
public:
	TorqueDemoHandler(task_thread_pool::task_thread_pool& pool): EntityHandler(pool) {}
};

int main() {
	try {
		Visualizer viz(1200, 800, "Boidsish - Torque and Force at Point Demo");

		Camera camera(0.0f, 5.0f, 25.0f, -10.0f, 0.0f, 0.0f);
		viz.SetCamera(camera);

		auto handler = std::make_shared<TorqueDemoHandler>(viz.GetThreadPool());

		// Use a shared_ptr with a no-op deleter for the stack-allocated Visualizer
		auto viz_ptr = std::shared_ptr<Visualizer>(&viz, [](Visualizer*) {});
		handler->vis = viz_ptr;

		viz.AddShapeHandler(std::ref(*handler));

		// Add some entities
		handler->AddEntity<DotEntity>(glm::vec3(-10.0f, 5.0f, 0.0f), 100.0f);
		handler->AddEntity<ModelEntity>("assets/utah_teapot.obj", glm::vec3(0.0f, 5.0f, 0.0f), 5.0f);
		handler->AddEntity<ModelEntity>("assets/Mesh_Cat.obj", glm::vec3(10.0f, 5.0f, 0.0f), 5.0f);

		// Input callback for clicking
		viz.AddInputCallback([&](const InputState& state) {
			if (state.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
				Ray       ray = viz.GetRayFromScreen(state.mouse_x, state.mouse_y);
				float     t;
				glm::vec3 hit_point;
				auto      entity = handler->RaycastEntities(ray, t, hit_point);

				if (entity) {
					// Apply a force in the direction of the ray at the hit point
					float     force_magnitude = 500.0f;
					glm::vec3 force = ray.direction * force_magnitude;
					entity->AddForceAtPoint(force, hit_point);

					std::cout << "Hit entity " << entity->GetId() << " at " << glm::to_string(hit_point) << std::endl;

					// Add a small temporary fire effect at the hit point for visual feedback
					viz.AddFireEffect(hit_point, FireEffectStyle::Sparks, ray.direction, glm::vec3(0), 50, 0.5f);
				}
			}
		});

		std::cout << "Torque Demo Started!" << std::endl;
		std::cout << "Click on objects to apply force and watch them spin!" << std::endl;

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
