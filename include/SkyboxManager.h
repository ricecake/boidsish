#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

namespace Boidsish {
    class SkyboxManager {
    public:
        SkyboxManager();
        ~SkyboxManager();

        void Render(const glm::mat4& projection, const glm::mat4& view);

    private:
        std::unique_ptr<Shader> shader_;
        GLuint vao_;
    };
}
