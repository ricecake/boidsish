#include <iostream>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics.h"
#include "dot.h"
#include "model.h"
#include "procedural_ir.h"
#include "procedural_mesher.h"
#include <ik/body.h>
#include <ik/solver.hpp>

using namespace Boidsish;

class IKTreeDemo {
public:
    IKTreeDemo(Visualizer& vis) : vis(vis) {
        // Create a spider-like model
        ProceduralIR ir;
        ir.name = "IK_Spider";

        int body_hub = ir.AddHub(glm::vec3(0, 0, 0), 0.5f, glm::vec3(0.2, 0.2, 0.2), -1, "body_hub");
        
        std::string leg_names[4] = {"FL", "FR", "BR", "BL"};
        glm::vec3 offsets[4] = {
            {0.5f, 0, 0.5f}, {0.5f, 0, -0.5f}, {-0.5f, 0, -0.5f}, {-0.5f, 0, 0.5f}
        };

        for(int i=0; i<4; ++i) {
            glm::vec3 end1 = offsets[i] * 2.0f;
            end1.y = 1.0f;
            int t1 = ir.AddTube(offsets[i], end1, 0.15f, 0.1f, glm::vec3(0.5, 0.5, 0.5), body_hub, leg_names[i] + "_upper");
            
            glm::vec3 end2 = end1 + offsets[i] * 1.5f;
            end2.y = -1.0f;
            ir.AddTube(end1, end2, 0.1f, 0.05f, glm::vec3(0.3, 0.3, 0.3), t1, leg_names[i] + "_lower");
        }

        auto base_model = ProceduralMesher::GenerateModel(ir);
        
        // Setup Skeleton
        auto data = std::make_shared<ModelData>();
        *data = *(base_model->GetData());
        data->bone_info_map.clear();
        data->bone_count = 0;
        data->root_node = NodeData();
        data->root_node.name = "SkeletonRoot";

        data->AddBone("body", "SkeletonRoot", glm::mat4(1.0f));
        for(int i=0; i<4; ++i) {
            glm::vec3 end1 = offsets[i] * 2.0f;
            end1.y = 1.0f;
            data->AddBone(leg_names[i] + "_upper", "body", glm::translate(glm::mat4(1.0f), offsets[i]));
            data->AddBone(leg_names[i] + "_lower", leg_names[i] + "_upper", glm::translate(glm::mat4(1.0f), end1 - offsets[i]));
            data->AddBone(leg_names[i] + "_foot", leg_names[i] + "_lower", glm::translate(glm::mat4(1.0f), (offsets[i] * 3.5f + glm::vec3(0, -1, 0)) - end1));
        }

        model = std::make_shared<Model>(data);
        model->SetPosition(0, 2, 0);
        model->UpdateAnimation(0);
        model->SkinToHierarchy();

        // Setup IK Body
        body.position = model->GetBoneWorldPosition("body");
        body.weight = 5.0f;

        for (int i = 0; i < 4; ++i) {
            Chain leg;
            leg.base = offsets[i];
            leg.hasTarget = true;

            Bone b_upper; b_upper.name = leg_names[i] + "_upper";
            Bone b_lower; b_lower.name = leg_names[i] + "_lower";
            Bone b_foot;  b_foot.name  = leg_names[i] + "_foot";

            leg.bones.push_back(b_upper);
            leg.bones.push_back(b_lower);
            leg.bones.push_back(b_foot);

            leg.target = model->GetBoneWorldPosition(b_foot.name);
            body.tree.chains.push_back(leg);
        }

        targetMarkers.resize(4);
        for(int i=0; i<4; ++i) {
            targetMarkers[i] = std::make_shared<Dot>(0,0,0, 1, 0, 0, 1);
            targetMarkers[i]->SetScale(glm::vec3(0.2f));
        }
    }

    void Update(float dt) {
        time += dt;

        // Move the body goal in a circle
        body.goal = glm::vec3(sin(time) * 2.0f, 2.5f + cos(time * 0.5f), cos(time) * 2.0f);

        // Animate feet targets
        for (int i = 0; i < 4; ++i) {
            float phase = (i / 2) * 3.14159f;
            float footX = (i < 2 ? 3.0f : -3.0f) + sin(time * 2.0f + phase) * 1.5f;
            float footZ = (i % 2 == 0 ? 3.0f : -3.0f) + cos(time * 2.0f + phase) * 1.5f;
            float footY = 0.0f + std::max(0.0f, (float)sin(time * 4.0f + phase)) * 1.0f;
            body.tree.chains[i].target = glm::vec3(footX, footY, footZ);
            targetMarkers[i]->SetPosition(footX, footY, footZ);
        }

        // Synchronize body world pos to model
        body.position = model->GetBoneWorldPosition("body");

        model->SolveIK(body, 20, 0.01f);

        // Update model root to follow the solved body position
        model->SetPosition(body.position.x, body.position.y, body.position.z);
    }

    Body body;
    std::shared_ptr<Model> model;
    std::vector<std::shared_ptr<Dot>> targetMarkers;

private:
    Visualizer& vis;
    float time = 0;
};

int main() {
    Visualizer vis(1280, 720, "IK Tree Demo - Model Integration");

    vis.AddPrepareCallback([&](Visualizer& v) {
        auto demo = std::make_shared<IKTreeDemo>(v);
        v.AddShape(demo->model);
        for (auto& target : demo->targetMarkers) v.AddShape(target);

        v.AddShapeHandler([demo, &vis](float dt) {
            demo->Update(vis.GetLastFrameTime());
            return std::vector<std::shared_ptr<Shape>>{};
        });
    });

    vis.Run();
    return 0;
}
