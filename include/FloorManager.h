#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>

class Shader;

namespace Boidsish {

class ResourceManager;

class FloorManager {
public:
    FloorManager(int width, int height);
    ~FloorManager();

    void Initialize(ResourceManager& resource_manager);
    void Resize(int width, int height);

    // Manages the pre-pass for rendering the reflection texture
    void BeginReflectionPass();
    void EndReflectionPass();

    void Render(const glm::mat4& view, const glm::mat4& projection);

    void SetReflectionViewProjection(const glm::mat4& vp) { reflection_vp_ = vp; }

    GLuint GetBlurQuadVAO() const { return blur_quad_vao_; }

private:
    void InitializeFBOs();
    void InitializeVAOs();
    void RenderBlur();

    int width_, height_;

    std::shared_ptr<Shader> plane_shader_;
    std::shared_ptr<Shader> blur_shader_;

    GLuint plane_vao_ = 0, plane_vbo_ = 0;
    GLuint blur_quad_vao_ = 0, blur_quad_vbo_ = 0;

    GLuint reflection_fbo_ = 0, reflection_texture_ = 0, reflection_depth_rbo_ = 0;
    GLuint pingpong_fbo_[2] = {0}, pingpong_texture_[2] = {0};

    glm::mat4 reflection_vp_;
};

} // namespace Boidsish
