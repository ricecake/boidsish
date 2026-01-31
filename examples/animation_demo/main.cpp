#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "model.h"
#include "animation.h"
#include "animator.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

class AnimatedEntity : public Model {
public:
    AnimatedEntity(const std::string& path) : Model(path) {
        m_Animation = std::make_unique<Animation>(path, this);
        m_Animator = std::make_unique<Animator>(m_Animation.get());
    }

    void Update(float dt) {
        m_Animator->UpdateAnimation(dt);
    }

    void render(Shader& shader, const glm::mat4& model_matrix) const override {
        auto transforms = m_Animator->GetFinalBoneMatrices();
        shader.setMat4Array("finalBonesMatrices", transforms);
        Model::render(shader, model_matrix);
    }

private:
    std::unique_ptr<Animation> m_Animation;
    std::unique_ptr<Animator> m_Animator;
};

int main() {
    try {
        Visualizer visualizer(1280, 720, "Boidsish - Animation Demo");

        auto bird = std::make_shared<AnimatedEntity>("assets/smolbird.fbx");
        bird->SetPosition(0.0f, 2.0f, 0.0f);
        bird->SetScale(glm::vec3(0.01f)); // FBX often needs scaling

        visualizer.AddPrepareCallback([bird](Visualizer& v) {
            v.AddShape(bird);
            v.GetCamera().x = 0;
            v.GetCamera().y = 2;
            v.GetCamera().z = 5;
            v.GetCamera().pitch = 0;
            v.GetCamera().yaw = 180;
        });

        visualizer.AddInputCallback([bird](const InputState& state) {
            bird->Update(state.delta_time);
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
