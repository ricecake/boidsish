#include <iostream>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "graphics.h"
#include "dot.h"
#include <ik/body.h>
#include <ik/solver.hpp>

using namespace Boidsish;

class IKTreeDemo {
public:
    IKTreeDemo(Visualizer& vis) : vis(vis) {
        // Setup a simple quadruped body
        body.position = glm::vec3(0, 2, 0);
        body.weight = 5.0f; // Heavy body

        // Add 4 legs
        for (int i = 0; i < 4; ++i) {
            Chain leg;
            float dx = (i < 2) ? 1.0f : -1.0f;
            float dz = (i % 2 == 0) ? 1.0f : -1.0f;
            leg.base = glm::vec3(dx, 0, dz);

            // Upper leg
            Bone upper;
            upper.name = "upper_" + std::to_string(i);
            upper.length = 1.2f;
            upper.weight = 1.0f;
            upper.isRelative = true;
            upper.relativePosition = glm::vec3(0, -1, 0);

            // Lower leg
            Bone lower;
            lower.name = "lower_" + std::to_string(i);
            lower.length = 1.2f;
            lower.weight = 0.5f;
            lower.isRelative = true;
            lower.relativePosition = glm::vec3(0, -1, 0);

            leg.bones.push_back(upper);
            leg.bones.push_back(lower);

            leg.target = body.position + leg.base + glm::vec3(0, -2, 0);
            leg.hasTarget = true;

            body.tree.chains.push_back(leg);
        }

        // Initialize world positions
        IKSolver::ResolveWorldPositions(body);

        // Visual representations
        bodyMarker = std::make_shared<Dot>(body.position.x, body.position.y, body.position.z, 1, 1, 1, 1);
        bodyMarker->SetScale(glm::vec3(0.5f));

        for (auto& chain : body.tree.chains) {
            std::vector<std::shared_ptr<Dot>> legDots;
            for (auto& bone : chain.bones) {
                auto dot = std::make_shared<Dot>(bone.position.x, bone.position.y, bone.position.z, 0, 1, 0, 1);
                dot->SetScale(glm::vec3(0.2f));
                legDots.push_back(dot);
            }
            boneMarkers.push_back(legDots);

            auto targetDot = std::make_shared<Dot>(chain.target.x, chain.target.y, chain.target.z, 1, 0, 0, 1);
            targetDot->SetScale(glm::vec3(0.15f));
            targetMarkers.push_back(targetDot);
        }
    }

    void Update(float dt) {
        time += dt;

        // Move the body goal in a circle
        body.goal = glm::vec3(sin(time) * 2.0f, 2.5f + cos(time * 0.5f), cos(time) * 2.0f);

        // Animate feet targets
        for (int i = 0; i < 4; ++i) {
            float phase = (i / 2) * 3.14159f;
            float footX = (i < 2 ? 1.5f : -1.5f) + sin(time * 2.0f + phase) * 0.5f;
            float footZ = (i % 2 == 0 ? 1.5f : -1.5f) + cos(time * 2.0f + phase) * 0.5f;
            float footY = 0.0f + std::max(0.0f, (float)sin(time * 4.0f + phase)) * 0.5f;
            body.tree.chains[i].target = glm::vec3(footX, footY, footZ);
        }

        IKSolver::Solve(body, 10, 0.01f);

        // Update visuals
        bodyMarker->SetPosition(body.position.x, body.position.y, body.position.z);
        for (size_t i = 0; i < body.tree.chains.size(); ++i) {
            for (size_t j = 0; j < body.tree.chains[i].bones.size(); ++j) {
                boneMarkers[i][j]->SetPosition(body.tree.chains[i].bones[j].position.x,
                                              body.tree.chains[i].bones[j].position.y,
                                              body.tree.chains[i].bones[j].position.z);
            }
            targetMarkers[i]->SetPosition(body.tree.chains[i].target.x,
                                         body.tree.chains[i].target.y,
                                         body.tree.chains[i].target.z);
        }
    }

    Body body;
    std::shared_ptr<Dot> bodyMarker;
    std::vector<std::vector<std::shared_ptr<Dot>>> boneMarkers;
    std::vector<std::shared_ptr<Dot>> targetMarkers;

private:
    Visualizer& vis;
    float time = 0;
};

int main() {
    Visualizer vis(1280, 720, "IK Tree Demo - Quadruped Balance");

    vis.AddPrepareCallback([&](Visualizer& v) {
        auto demo = std::make_shared<IKTreeDemo>(v);
        v.AddShape(demo->bodyMarker);
        for (auto& leg : demo->boneMarkers) {
            for (auto& dot : leg) v.AddShape(dot);
        }
        for (auto& target : demo->targetMarkers) v.AddShape(target);

        v.AddShapeHandler([demo, &vis](float dt) {
            demo->Update(vis.GetLastFrameTime());
            return std::vector<std::shared_ptr<Shape>>{};
        });
    });

    vis.Run();
    return 0;
}
