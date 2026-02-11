#include "akira_effect.h"
#include <shader.h>
#include "shape.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

namespace Boidsish {

    AkiraEffectManager::AkiraEffectManager() {
        shader_ = std::make_unique<Shader>("shaders/akira.vert", "shaders/akira.frag");
    }

    AkiraEffectManager::~AkiraEffectManager() = default;

    void AkiraEffectManager::Trigger(const glm::vec3& position, float radius) {
        effects_.emplace_back(position, radius);
    }

    void AkiraEffectManager::Update(float delta_time, ITerrainGenerator& terrain) {
        auto it = effects_.begin();
        while (it != effects_.end()) {
            it->elapsed_time += delta_time;

            if (it->phase == AkiraEffect::Phase::GROWING) {
                if (it->elapsed_time >= it->growth_duration) {
                    it->phase = AkiraEffect::Phase::FADING;
                }
            }

            // Trigger deformation slightly before full growth to ensure it's obscured
            if (!it->deformation_triggered && it->GetGrowthProgress() >= 0.95f) {
                terrain.AddAkira(it->center, it->radius);
                it->deformation_triggered = true;
            }

            if (it->phase == AkiraEffect::Phase::FADING && it->GetFadeProgress() >= 1.0f) {
                it->phase = AkiraEffect::Phase::FINISHED;
            }

            if (it->phase == AkiraEffect::Phase::FINISHED) {
                it = effects_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void AkiraEffectManager::Render(const glm::mat4& view, const glm::mat4& projection, float /* time */) {
        if (effects_.empty()) return;

        shader_->use();
        shader_->setMat4("view", view);
        shader_->setMat4("projection", projection);

        glBindVertexArray(Shape::sphere_vao_);

        for (const auto& effect : effects_) {
            float current_radius;
            if (effect.phase == AkiraEffect::Phase::GROWING) {
                current_radius = effect.radius * effect.GetGrowthProgress();
            } else {
                current_radius = effect.radius;
            }

            glm::mat4 model = glm::translate(glm::mat4(1.0f), effect.center);
            model = glm::scale(model, glm::vec3(current_radius));
            shader_->setMat4("model", model);

            shader_->setFloat("growthProgress", effect.GetGrowthProgress());
            shader_->setFloat("fadeProgress", effect.GetFadeProgress());
            shader_->setInt("phase", static_cast<int>(effect.phase));

            glDrawElements(GL_TRIANGLES, Shape::sphere_vertex_count_, GL_UNSIGNED_INT, 0);
        }

        glBindVertexArray(0);
    }

} // namespace Boidsish
