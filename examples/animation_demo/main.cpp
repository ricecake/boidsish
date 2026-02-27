#include "graphics.h"
#include "model.h"
#include "animator.h"
#include "asset_manager.h"
#include <iostream>
#include <vector>

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1280, 720, "Animation Demo");

		// Try to load the bird
		auto model_path = "/home/ricecake/Documents/Assets/Models/anim/Cow.fbx";
		auto bird = std::make_shared<Model>(model_path);

		bird->SetPosition(0.0f, 5.0f, 0.0f);
		bird->SetScale(glm::vec3(0.10f));
		bird->SetAnimation(0);
		viz.AddShape(bird);

		// // Add a floor for reference
		// auto floor = std::make_shared<Model>("assets/quad.obj");
		// floor->SetPosition(0.0f, 0.0f, 0.0f);
		// floor->SetScale(glm::vec3(100.0f, 1.0f, 100.0f));
		// viz.AddShape(floor);

		// // Add a light to see something
		// Light sun = Light::CreateDirectional(45.0f, 45.0f, 1.5f, glm::vec3(1.0f, 0.9f, 0.8f));
		// viz.GetLightManager().AddLight(sun);

		// viz.AddPrepareCallback([](Visualizer& v) {
		// 	v.GetCamera().x = 0.0f;
		// 	v.GetCamera().y = 10.0f;
		// 	v.GetCamera().z = 25.0f;
		// 	v.GetCamera().pitch = -15.0f;
		// 	v.GetCamera().yaw = 0.0f;
		// });

		static float animTimer = 0.0f;
		static int currentAnim = 0;

		viz.AddShapeHandler([&](float) {
			auto dt = viz.GetLastFrameTime();
			bird->UpdateAnimation(dt);
			// Slow rotate bird
			bird->SetRotation(glm::angleAxis(dt * 0.5f, glm::vec3(0, 1, 0)) * bird->GetRotation());

			// Cycle animations every 5 seconds if multiple exist
			animTimer += dt;
			if (animTimer > 5.0f) {
				animTimer = 0.0f;
				auto data = AssetManager::GetInstance().GetModelData(model_path);
				if (data && !data->animations.empty()) {
					currentAnim = (currentAnim + 1) % data->animations.size();
					bird->SetAnimation(currentAnim);
					std::cout << "Switched to animation [" << currentAnim << "]: " << data->animations[currentAnim].name << std::endl;
				}
			}

			return std::vector<std::shared_ptr<Shape>>{};
		});

		std::cout << "Starting Animation Demo..." << std::endl;
		std::cout << "Model: " << model_path << std::endl;

		auto data = AssetManager::GetInstance().GetModelData(model_path);
		if (data) {
			std::cout << "Model loaded with " << data->meshes.size() << " meshes." << std::endl;
			std::cout << "Bone count: " << data->bone_count << std::endl;
			std::cout << "Animations found: " << data->animations.size() << std::endl;
			for (size_t i = 0; i < data->animations.size(); i++) {
				std::cout << "  [" << i << "] " << data->animations[i].name << " (Duration: " << data->animations[i].duration << ")" << std::endl;
			}

			AABB aabb = data->aabb;
			std::cout << "Model AABB Min: (" << aabb.min.x << ", " << aabb.min.y << ", " << aabb.min.z << ")" << std::endl;
			std::cout << "Model AABB Max: (" << aabb.max.x << ", " << aabb.max.y << ", " << aabb.max.z << ")" << std::endl;

			// Print first 5 bone names if they exist
			int count = 0;
			std::cout << "First few bones:" << std::endl;
			for (auto const& [name, info] : data->bone_info_map) {
				std::cout << "  - " << name << " (ID: " << info.id << ")" << std::endl;
				if (++count >= 5) break;
			}
		}

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
