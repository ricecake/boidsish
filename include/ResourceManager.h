#pragma once

#include <map>
#include <memory>
#include <string>

#include <GL/glew.h>
#include <shader.h>

namespace Boidsish {

    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();

        void Initialize();

        std::shared_ptr<Shader> GetShader(const std::string& name);
        GLuint GetLightingUBO() const { return lighting_ubo_; }
        GLuint GetVisualEffectsUBO() const { return visual_effects_ubo_; }

    private:
        void LoadShaders();
        void CreateUBOs();
        void SetupShaderBindings(Shader& shader);

        std::map<std::string, std::shared_ptr<Shader>> shaders_;
        GLuint                                         lighting_ubo_;
        GLuint                                         visual_effects_ubo_;
    };

} // namespace Boidsish
