#include "ResourceManager.h"
#include "visual_effects.h"

namespace Boidsish {

    ResourceManager::ResourceManager()
        : lighting_ubo_(0), visual_effects_ubo_(0) {}

    ResourceManager::~ResourceManager() {
        glDeleteBuffers(1, &lighting_ubo_);
        glDeleteBuffers(1, &visual_effects_ubo_);
    }

    void ResourceManager::Initialize() {
        CreateUBOs();
        LoadShaders();
    }

    std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& name) {
        if (shaders_.find(name) != shaders_.end()) {
            return shaders_[name];
        }
        return nullptr;
    }

    void ResourceManager::LoadShaders() {
        shaders_["boids"] = std::make_shared<Shader>("shaders/vis.vert", "shaders/vis.frag");
        shaders_["trail"] = std::make_shared<Shader>("shaders/trail.vert", "shaders/trail.frag");
        shaders_["plane"] = std::make_shared<Shader>("shaders/plane.vert", "shaders/plane.frag");
        shaders_["sky"] = std::make_shared<Shader>("shaders/sky.vert", "shaders/sky.frag");
        shaders_["blur"] = std::make_shared<Shader>("shaders/blur.vert", "shaders/blur.frag");
        shaders_["postprocess"] = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
        shaders_["terrain"] = std::make_shared<Shader>(
            "shaders/terrain.vert",
            "shaders/terrain.frag",
            "shaders/terrain.tcs",
            "shaders/terrain.tes"
        );

        for (auto const& [key, val] : shaders_) {
            SetupShaderBindings(*val);
        }
    }

    void ResourceManager::CreateUBOs() {
        glGenBuffers(1, &lighting_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, 48, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, lighting_ubo_, 0, 48);

        glGenBuffers(1, &visual_effects_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(VisualEffectsUbo), NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, visual_effects_ubo_, 0, sizeof(VisualEffectsUbo));
    }

    void ResourceManager::SetupShaderBindings(Shader& shader) {
        shader.use();
        glUniformBlockBinding(shader.ID, glGetUniformBlockIndex(shader.ID, "Lighting"), 0);
        glUniformBlockBinding(shader.ID, glGetUniformBlockIndex(shader.ID, "VisualEffects"), 1);
    }

} // namespace Boidsish
