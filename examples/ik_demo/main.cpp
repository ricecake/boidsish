#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "model.h"
#include "procedural_generator.h"
#include "procedural_ir.h"
#include "procedural_mesher.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

class IKDemo {
public:
	IKDemo(Visualizer& vis): vis(vis) {
		// Create procedural model
		ProceduralIR ir;
		ir.name = "IK_Tube";

		// Root hub
		int root = ir.AddHub(glm::vec3(0, 0, 0), 0.2f, glm::vec3(0.5, 0.5, 0.5));

		// Chain of tubes
		int t1 = ir.AddTube(glm::vec3(0, 0, 0), glm::vec3(0, 1.5, 0), 0.15f, 0.12f, glm::vec3(0.8, 0.4, 0.1), root);
		int t2 = ir.AddTube(glm::vec3(0, 1.5, 0), glm::vec3(0, 3.0, 0), 0.12f, 0.1f, glm::vec3(0.8, 0.4, 0.1), t1);
		int t3 = ir.AddTube(glm::vec3(0, 3.0, 0), glm::vec3(0, 4.5, 0), 0.1f, 0.08f, glm::vec3(0.8, 0.4, 0.1), t2);

		// Effector ball
		ir.AddPuffball(glm::vec3(0, 4.5, 0), 0.3f, glm::vec3(1.0, 0.2, 0.2), t3);

		model = ProceduralMesher::GenerateModel(ir);
		model->SetPosition(0, 0, 0);

		// Setup bone names based on ProceduralMesher logic (if it generated them)
		// Since we are using manual AddBone for clarity in this demo, let's redefine the skeleton
		auto data = std::make_shared<ModelData>();
		*data = *(model->GetData());

		// Clear automatic bones if any and add ours
		data->bone_info_map.clear();
		data->bone_count = 0;
		data->root_node = NodeData();
		data->root_node.name = "SkeletonRoot";

		data->AddBone("bone_root", "SkeletonRoot", glm::mat4(1.0f));
		data->AddBone("bone_mid1", "bone_root", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5, 0)));
		data->AddBone("bone_mid2", "bone_mid1", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5, 0)));
		data->AddBone("bone_effector", "bone_mid2", glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5, 0)));

		model = std::make_shared<Model>(data);
		model->SetPosition(0, 0, 0);
		model->UpdateAnimation(0);
		model->SkinToHierarchy();

		// // Apply constraints
		// BoneConstraint cone;
		// cone.type = ConstraintType::Cone;
		// cone.coneAngle = 60.0f;
		// model->SetBoneConstraint("bone_root", cone);
		// model->SetBoneConstraint("bone_mid1", cone);
		// model->SetBoneConstraint("bone_mid2", cone);

		targetMarker = std::make_shared<Dot>(0, 0, 0, 1, 1, 0, 1);
		targetMarker->SetScale(glm::vec3(0.2f));
		targetMarker->SetHidden(true);

		startPos = model->GetBoneWorldPosition("bone_effector");
		currentTarget = startPos;
	}

	void Update(float dt) {
		if (animating) {
			animTime += dt;
			float t = glm::clamp(animTime / duration, 0.0f, 1.0f);

			// Linear interpolation + Arched path
			glm::vec3 p = glm::mix(animStart, animEnd, t);
			float     arch = 1.0f * std::sin(t * 3.14159f); // Upward arch
			p.y += arch;

			currentTarget = p;

			if (t >= 1.0f) {
				animating = false;
			}
		}

		model->SolveIK("bone_effector", currentTarget, 0.01f, 20, "bone_root");
		model->UpdateAnimation(dt);
	}

	void SetTarget(glm::vec3 pos) {
		model->ResetBones(); // Reset to avoid accumulation errors and skewed scales
		animStart = model->GetBoneWorldPosition("bone_effector");
		animEnd = pos;
		animTime = 0;
		animating = true;

		targetMarker->SetPosition(pos.x, pos.y, pos.z);
		targetMarker->SetHidden(false);
	}

	std::shared_ptr<Model> model;
	std::shared_ptr<Dot>   targetMarker;

private:
	Visualizer& vis;
	bool        animating = false;
	float       animTime = 0;
	float       duration = 1.0f;
	glm::vec3   animStart;
	glm::vec3   animEnd;
	glm::vec3   startPos;
	glm::vec3   currentTarget;
};

int main() {
	Visualizer vis(1280, 720, "IK Demo - Click Terrain to Move Effector");

	vis.AddPrepareCallback([&](Visualizer& v) {
		auto demo = std::make_shared<IKDemo>(v);
		v.AddShape(demo->model);
		v.AddShape(demo->targetMarker);

		v.AddInputCallback([&, demo](const InputState& input) {
			if (input.mouse_button_down[0]) {
				auto worldPos = v.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (worldPos) {
					demo->SetTarget(*worldPos);
				}
			}
		});

		v.AddShapeHandler([demo, &vis](float dt) {
			demo->Update(vis.GetLastFrameTime());
			return std::vector<std::shared_ptr<Shape>>{};
		});
	});

	vis.Run();
	return 0;
}
