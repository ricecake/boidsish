#include "graphics.h"
#include "model.h"
#include "animator.h"
#include "asset_manager.h"
#include <iostream>

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1280, 720, "Animation Demo");

		// Try to load the bird, fallback to teapot if it fails to show anything
		auto model_path = "assets/smolbird.fbx";
		auto bird = std::make_shared<Model>(model_path);

		// FBX often needs small scale, but let's try 1.0 first if it's not showing
		bird->SetPosition(0.0f, 10.0f, 0.0f);
		bird->SetScale(glm::vec3(1.0f));
		bird->SetAnimation(0);
		viz.AddShape(bird);

		// Add a floor for reference
		auto floor = std::make_shared<Model>("assets/quad.obj");
		floor->SetPosition(0.0f, 0.0f, 0.0f);
		floor->SetScale(glm::vec3(100.0f, 1.0f, 100.0f));
		viz.AddShape(floor);

		// Add a light to see something (azimuth 45, elevation 45)
		Light sun = Light::CreateDirectional(45.0f, 45.0f, 1.5f, glm::vec3(1.0f, 0.9f, 0.8f));
		viz.GetLightManager().AddLight(sun);

		viz.AddPrepareCallback([](Visualizer& v) {
			v.GetCamera().x = 0.0f;
			v.GetCamera().y = 15.0f;
			v.GetCamera().z = 30.0f;
			v.GetCamera().pitch = -15.0f;
			v.GetCamera().yaw = 0.0f;
		});

		viz.AddShapeHandler([&](float /* time */) {
			bird->UpdateAnimation(viz.GetLastFrameTime());
			// Slow rotate bird to see all sides
			bird->SetRotation(glm::angleAxis(viz.GetLastFrameTime() * 0.5f, glm::vec3(0, 1, 0)) * bird->GetRotation());
			return std::vector<std::shared_ptr<Shape>>{};
		});

		std::cout << "Starting Animation Demo..." << std::endl;
		std::cout << "Model: " << model_path << std::endl;

		if (bird->getMeshes().empty()) {
			std::cout << "WARNING: Model has no meshes!" << std::endl;
		} else {
			std::cout << "Model loaded with " << bird->getMeshes().size() << " meshes." << std::endl;
		}

		// Diagnostics
		AABB aabb = bird->GetAABB();
		std::cout << "Model AABB Min: (" << aabb.min.x << ", " << aabb.min.y << ", " << aabb.min.z << ")" << std::endl;
		std::cout << "Model AABB Max: (" << aabb.max.x << ", " << aabb.max.y << ", " << aabb.max.z << ")" << std::endl;

		auto animator = bird->GetAnimator();
		if (animator) {
			auto data = AssetManager::GetInstance().GetModelData(model_path);
			if (data) {
				std::cout << "Animations found: " << data->animations.size() << std::endl;
				for (size_t i = 0; i < data->animations.size(); i++) {
					std::cout << "  [" << i << "] " << data->animations[i].name << std::endl;
				}
			}
		}

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
