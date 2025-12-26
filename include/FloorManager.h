#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;

namespace Boidsish {
    class FloorManager {
    public:
        FloorManager(int width, int height);
        ~FloorManager();

        void Render(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& reflection_vp);
        void BeginReflectionPass();
        void EndReflectionPass();
        void BlurReflection(int passes);
        void Resize(int width, int height);

        GLuint GetReflectionTexture() const { return reflection_texture_; }

    private:
        void SetupPlaneMesh();
        void SetupFBOs(int width, int height);

        int width_, height_;
        std::unique_ptr<Shader> plane_shader_;
        std::unique_ptr<Shader> blur_shader_;

        GLuint plane_vao_, plane_vbo_;
        GLuint blur_quad_vao_, blur_quad_vbo_;

        GLuint reflection_fbo_, reflection_texture_, reflection_depth_rbo_;
        GLuint pingpong_fbo_[2], pingpong_texture_[2];
    };
}
