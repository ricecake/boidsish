#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cmath>
#include <map>

#include "graphics.h"
#include "model.h"
#include "dot.h"

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1280, 720, "IK Limb Animation Demo");

		// Load an existing model. We can load the cat model by default.
		std::string model_path = "assets/Mesh_Cat.obj";
		auto model = std::make_shared<Model>(model_path);

		// Position and scale the model nicely
		model->SetPosition(0.0f, 0.0f, 0.0f);
		model->SetScale(glm::vec3(0.2f));

		// Check if the loaded model already has bone/skeletal information.
		// If it has none, we'll programmatically rig and skin a skeleton onto it.
		auto data = model->GetData();
		bool model_has_existing_bones = data && !data->bone_info_map.empty();

		if (!model_has_existing_bones) {
			std::cout << "[LOG] Model has no existing bones. Programmatically rigging 4 limbs onto " << model_path << "..." << std::endl;

			// Set up custom bones on the cat model
			auto newData = std::make_shared<ModelData>();
			*newData = *data;

			newData->bone_info_map.clear();
			newData->bone_count = 0;
			newData->root_node = NodeData();
			newData->root_node.name = "SkeletonRoot";

			// Bone setup hierarchy: Root at center hip height
			// We define limbs for Front-Left, Front-Right, Back-Left, Back-Right.
			// Cat dimensions (AABB): X: -8.74 to 8.74, Y: -35.64 to 35.64, Z: -42.13 to 42.13.
			// Note that the Cat model is positioned centered on the ground, so let's put the root around Y = 10.0f.
			newData->AddBone("bone_root", "SkeletonRoot", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)));

			// Front Left Leg (X is negative, Z is positive)
			newData->AddBone("bone_fl_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 25.0f)));
			newData->AddBone("bone_fl_knee", "bone_fl_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
			newData->AddBone("bone_fl_foot", "bone_fl_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

			// Front Right Leg (X is positive, Z is positive)
			newData->AddBone("bone_fr_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, 25.0f)));
			newData->AddBone("bone_fr_knee", "bone_fr_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
			newData->AddBone("bone_fr_foot", "bone_fr_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

			// Back Left Leg (X is negative, Z is negative)
			newData->AddBone("bone_bl_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, -25.0f)));
			newData->AddBone("bone_bl_knee", "bone_bl_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
			newData->AddBone("bone_bl_foot", "bone_bl_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

			// Back Right Leg (X is positive, Z is negative)
			newData->AddBone("bone_br_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -25.0f)));
			newData->AddBone("bone_br_knee", "bone_br_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
			newData->AddBone("bone_br_foot", "bone_br_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

			// Recreate the model with our newly defined skeleton
			model = std::make_shared<Model>(newData);
			model->SetPosition(0.0f, 0.0f, 0.0f);
			model->SetScale(glm::vec3(0.2f));
			model->UpdateAnimation(0.0f);
			model->SkinToHierarchy();
		} else {
			std::cout << "[LOG] Model already has existing bone skeleton (" << data->bone_info_map.size() << " bones). Keeping original bone structure." << std::endl;
		}

		viz.AddShape(model);

		// Get all terminal effectors from the bone hierarchy
		std::vector<std::string> effectors = model->GetEffectors();
		std::cout << "[LOG] Found " << effectors.size() << " terminal effector(s):" << std::endl;
		for (const auto& eff : effectors) {
			std::cout << "  - " << eff << std::endl;
		}

		// Setup visualization dot markers for each foot/effector dynamically
		std::vector<std::shared_ptr<Dot>> targets;
		for (size_t i = 0; i < effectors.size(); ++i) {
			auto dot = std::make_shared<Dot>(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
			dot->SetScale(glm::vec3(0.5f));
			viz.AddShape(dot);
			targets.push_back(dot);
		}

		// Calculate animation displacement distances relative to the model's physical size
		AABB aabb = model->GetAABB();
		float ext_height = aabb.max.y - aabb.min.y;
		if (ext_height < 0.1f) ext_height = 100.0f; // Safe fallback if AABB is flat/uninitialized

		float lift_distance = ext_height * 0.15f;    // Lift leg 15% of total height in the air
		float ground_distance = ext_height * 0.10f;  // Lower leg 10% of total height onto the ground

		// Time parameters:
		// Move each effector sequentially.
		// Animation sequence per effector:
		// Phase 1 (1.0s): Lift effector up (into the air)
		// Phase 2 (1.0s): Lower effector down (to the ground)
		// Phase 3 (1.0s): Move effector back to its original starting position
		const float phase_duration = 1.0f;
		const float animation_cycle = phase_duration * 3.0f; // 3 seconds per effector

		static float elapsed_time = 0.0f;

		viz.AddShapeHandler([&](float) {
			float dt = viz.GetLastFrameTime();
			elapsed_time += dt;

			int num_effectors = effectors.size();
			if (num_effectors == 0) return std::vector<std::shared_ptr<Shape>>{};

			// Calculate which effector is active and its progress
			float total_animation_duration = num_effectors * animation_cycle;
			float progress_time = std::fmod(elapsed_time, total_animation_duration);

			int active_effector_idx = static_cast<int>(progress_time / animation_cycle);
			float effector_time = std::fmod(progress_time, animation_cycle);

			// Rotate the model absolute-wise based on total elapsed time, preventing quadratic speed acceleration
			model->SetRotation(glm::angleAxis(elapsed_time * 0.3f, glm::vec3(0.0f, 1.0f, 0.0f)));

			// Reset bones and calculate the bone rest positions in the CURRENT frame (respecting the body's new rotation)
			model->ResetBones();
			model->UpdateAnimation(0.0f);

			// Solve IK for each effector
			for (int i = 0; i < num_effectors; ++i) {
				std::string effector_name = effectors[i];

				// Retrieve the un-deformed, un-IK'ed world rest position of the bone for this frame
				glm::vec3 start_pos = model->GetBoneWorldPosition(effector_name);
				glm::vec3 current_target = start_pos;

				if (i == active_effector_idx) {
					// Apply animation cycle to the active effector
					float phase = effector_time / phase_duration;

					if (phase < 1.0f) {
						// Phase 1: Lift effector up into the air
						float t = phase;
						glm::vec3 end_pos = start_pos + glm::vec3(0.0f, lift_distance, 0.0f);
						current_target = glm::mix(start_pos, end_pos, t);
					} else if (phase < 2.0f) {
						// Phase 2: Lower effector to the ground (below starting position)
						float t = phase - 1.0f;
						glm::vec3 lift_pos = start_pos + glm::vec3(0.0f, lift_distance, 0.0f);
						glm::vec3 ground_pos = start_pos - glm::vec3(0.0f, ground_distance, 0.0f);
						current_target = glm::mix(lift_pos, ground_pos, t);
					} else {
						// Phase 3: Move effector back to starting position
						float t = phase - 2.0f;
						glm::vec3 ground_pos = start_pos - glm::vec3(0.0f, ground_distance, 0.0f);
						current_target = glm::mix(ground_pos, start_pos, t);
					}
				}

				// Move target marker to the computed target position
				targets[i]->SetPosition(current_target.x, current_target.y, current_target.z);

				// Solve IK on the model
				model->SolveIK(effector_name, current_target, 0.01f, 20);
			}

			model->UpdateAnimation(dt);

			return std::vector<std::shared_ptr<Shape>>{};
		});

		// Add a directional light to see the model
		Light sun = Light::CreateDirectional(45.0f, 45.0f, 1.5f, glm::vec3(1.0f, 0.9f, 0.8f));
		viz.GetLightManager().AddLight(sun);

		// Initialize visualizer camera position
		viz.AddPrepareCallback([](Visualizer& v) {
			v.GetCamera().x = 0.0f;
			v.GetCamera().y = 15.0f;
			v.GetCamera().z = 80.0f;
			v.GetCamera().pitch = -10.0f;
			v.GetCamera().yaw = 180.0f; // Look back at the origin where the model will face
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
