#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>

class Shader;

namespace Boidsish {

    class ResourceManager;

    class SkyboxManager {
    public:
        SkyboxManager();
        ~SkyboxManager();

        void Initialize(ResourceManager& resourceManager);
        void Render(const glm::mat4& view, const glm::mat4& projection);

    private:
        std::shared_ptr<Shader> shader_;
        GLuint vao_;
    };

} // namespace Boidsish
