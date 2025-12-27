#include "FloorManager.h"
#include "ResourceManager.h"
#include "shader.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

constexpr int kBlurPasses = 4;

FloorManager::FloorManager(int width, int height) : width_(width), height_(height) {}

FloorManager::~FloorManager() {
    glDeleteVertexArrays(1, &plane_vao_);
    glDeleteBuffers(1, &plane_vbo_);
    glDeleteVertexArrays(1, &blur_quad_vao_);
    glDeleteBuffers(1, &blur_quad_vbo_);
    glDeleteFramebuffers(1, &reflection_fbo_);
    glDeleteTextures(1, &reflection_texture_);
    glDeleteRenderbuffers(1, &reflection_depth_rbo_);
    glDeleteFramebuffers(2, pingpong_fbo_);
    glDeleteTextures(2, pingpong_texture_);
}

void FloorManager::Initialize(ResourceManager& resource_manager) {
    plane_shader_ = resource_manager.GetShader("plane");
    blur_shader_ = resource_manager.GetShader("blur");
    InitializeVAOs();
    InitializeFBOs();
}

void FloorManager::Resize(int width, int height) {
    width_ = width;
    height_ = height;

    // Resize FBO textures
    glBindTexture(GL_TEXTURE_2D, reflection_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindRenderbuffer(GL_RENDERBUFFER, reflection_depth_rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);

    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, pingpong_texture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
    }
}

void FloorManager::BeginReflectionPass() {
    glEnable(GL_CLIP_DISTANCE0);
    glBindFramebuffer(GL_FRAMEBUFFER, reflection_fbo_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FloorManager::EndReflectionPass() {
    glDisable(GL_CLIP_DISTANCE0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    RenderBlur();
}

void FloorManager::Render(const glm::mat4& view, const glm::mat4& projection) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    plane_shader_->use();
    plane_shader_->setInt("reflectionTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pingpong_texture_[0]);
    plane_shader_->setMat4("reflectionViewProjection", reflection_vp_);

    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(600.0f));
    plane_shader_->setMat4("model", model);
    plane_shader_->setMat4("view", view);
    plane_shader_->setMat4("projection", projection);

    glBindVertexArray(plane_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void FloorManager::InitializeFBOs() {
    // Reflection FBO
    glGenFramebuffers(1, &reflection_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, reflection_fbo_);
    glGenTextures(1, &reflection_texture_);
    glBindTexture(GL_TEXTURE_2D, reflection_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflection_texture_, 0);
    glGenRenderbuffers(1, &reflection_depth_rbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, reflection_depth_rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflection_depth_rbo_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Reflection FBO is not complete!" << std::endl;

    // Ping-pong FBOs for blurring
    glGenFramebuffers(2, pingpong_fbo_);
    glGenTextures(2, pingpong_texture_);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
        glBindTexture(GL_TEXTURE_2D, pingpong_texture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_texture_[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong FBO is not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FloorManager::InitializeVAOs() {
    // Plane VAO
    float quad_vertices[] = {
        -1.0f, 0.0f,  1.0f,
         1.0f, 0.0f, -1.0f,
        -1.0f, 0.0f, -1.0f,
        -1.0f, 0.0f,  1.0f,
         1.0f, 0.0f,  1.0f,
         1.0f, 0.0f, -1.0f
    };
    glGenVertexArrays(1, &plane_vao_);
    glBindVertexArray(plane_vao_);
    glGenBuffers(1, &plane_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, plane_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Blur Quad VAO (shared with post-processing)
    float blur_quad_vertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &blur_quad_vao_);
    glBindVertexArray(blur_quad_vao_);
    glGenBuffers(1, &blur_quad_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, blur_quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blur_quad_vertices), blur_quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void FloorManager::RenderBlur() {
    glDisable(GL_DEPTH_TEST);
    blur_shader_->use();
    bool horizontal = true, first_iteration = true;
    for (int i = 0; i < kBlurPasses; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[horizontal]);
        blur_shader_->setInt("horizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? reflection_texture_ : pingpong_texture_[!horizontal]);
        glBindVertexArray(blur_quad_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration)
            first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

} // namespace Boidsish
