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
		Visualizer viz(1280, 720, "IK Limb Animation Demo - Cat");

		// Load the cat model
		std::string cat_path = "assets/Mesh_Cat.obj";
		auto cat = std::make_shared<Model>(cat_path);

		// Position and scale the cat nicely
		cat->SetPosition(0.0f, 0.0f, 0.0f);
		cat->SetScale(glm::vec3(0.2f));

		// Set up custom bones on the cat model
		auto data = std::make_shared<ModelData>();
		*data = *(cat->GetData());

		// Clear automatic/default empty bones if any
		data->bone_info_map.clear();
		data->bone_count = 0;
		data->root_node = NodeData();
		data->root_node.name = "SkeletonRoot";

		// Bone setup hierarchy: Root at center hip height
		// We define limbs for Front-Left, Front-Right, Back-Left, Back-Right.
		// Cat dimensions (AABB): X: -8.74 to 8.74, Y: -35.64 to 35.64, Z: -42.13 to 42.13.
		// Note that the Cat model is positioned centered on the ground, so let's put the root around Y = 10.0f.
		data->AddBone("bone_root", "SkeletonRoot", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)));

		// Front Left Leg (X is negative, Z is positive)
		data->AddBone("bone_fl_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 25.0f)));
		data->AddBone("bone_fl_knee", "bone_fl_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
		data->AddBone("bone_fl_foot", "bone_fl_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

		// Front Right Leg (X is positive, Z is positive)
		data->AddBone("bone_fr_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, 25.0f)));
		data->AddBone("bone_fr_knee", "bone_fr_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
		data->AddBone("bone_fr_foot", "bone_fr_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

		// Back Left Leg (X is negative, Z is negative)
		data->AddBone("bone_bl_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, -25.0f)));
		data->AddBone("bone_bl_knee", "bone_bl_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
		data->AddBone("bone_bl_foot", "bone_bl_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

		// Back Right Leg (X is positive, Z is negative)
		data->AddBone("bone_br_hip", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -25.0f)));
		data->AddBone("bone_br_knee", "bone_br_hip", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));
		data->AddBone("bone_br_foot", "bone_br_knee", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -22.5f, 0.0f)));

		// Recreate the model with our newly defined skeleton
		cat = std::make_shared<Model>(data);
		cat->SetPosition(0.0f, 0.0f, 0.0f);
		cat->SetScale(glm::vec3(0.2f));
		cat->UpdateAnimation(0.0f);
		cat->SkinToHierarchy();

		viz.AddShape(cat);

		// Setup visualization dot markers for each foot
		std::vector<std::shared_ptr<Dot>> targets;
		for (int i = 0; i < 4; ++i) {
			auto dot = std::make_shared<Dot>(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
			dot->SetScale(glm::vec3(0.5f));
			viz.AddShape(dot);
			targets.push_back(dot);
		}

		// Save starting positions in world space
		std::vector<std::string> effectors = cat->GetEffectors();
		std::vector<glm::vec3> start_positions;
		for (const auto& eff : effectors) {
			start_positions.push_back(cat->GetBoneWorldPosition(eff));
		}

		// Cycle animations sequently
		// Time parameters:
		// Move each effector sequentially.
		// Animation sequence per effector:
		// Phase 1 (1.0s): Lift leg up (into the air)
		// Phase 2 (1.0s): Lower leg down to the ground
		// Phase 3 (1.0s): Move leg back to original starting position
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

			// Rotate the cat absolute-wise based on total elapsed time, preventing quadratic speed acceleration
			cat->SetRotation(glm::angleAxis(elapsed_time * 0.3f, glm::vec3(0.0f, 1.0f, 0.0f)));

			cat->ResetBones();

			// Solve IK for each effector
			for (int i = 0; i < num_effectors; ++i) {
				std::string effector_name = effectors[i];
				glm::vec3 start_pos = start_positions[i];
				glm::vec3 current_target = start_pos;

				if (i == active_effector_idx) {
					// Apply animation cycle to the active effector
					float phase = effector_time / phase_duration;

					if (phase < 1.0f) {
						// Phase 1: Lift effector up into the air
						float t = phase;
						glm::vec3 end_pos = start_pos + glm::vec3(0.0f, 15.0f, 0.0f);
						current_target = glm::mix(start_pos, end_pos, t);
					} else if (phase < 2.0f) {
						// Phase 2: Lower effector to the ground (below starting position)
						float t = phase - 1.0f;
						glm::vec3 lift_pos = start_pos + glm::vec3(0.0f, 15.0f, 0.0f);
						glm::vec3 ground_pos = start_pos + glm::vec3(0.0f, -10.0f, 0.0f);
						current_target = glm::mix(lift_pos, ground_pos, t);
					} else {
						// Phase 3: Move effector back to starting position
						float t = phase - 2.0f;
						glm::vec3 ground_pos = start_pos + glm::vec3(0.0f, -10.0f, 0.0f);
						current_target = glm::mix(ground_pos, start_pos, t);
					}
				}

				// Move target marker to the computed target position
				targets[i]->SetPosition(current_target.x, current_target.y, current_target.z);

				// Solve IK on the model
				cat->SolveIK(effector_name, current_target, 0.01f, 20);
			}

			cat->UpdateAnimation(dt);

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
			v.GetCamera().yaw = 180.0f; // Look back at the origin where the cat will face
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
