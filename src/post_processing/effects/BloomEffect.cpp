#include "post_processing/effects/BloomEffect.h"

#include <iostream>

#include "logger.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

BloomEffect::BloomEffect(int width, int height)
    : _width(width), _height(height), _brightPassFBO(0), _brightPassTexture(0) {
    _pingpongFBO[0] = 0;
    _pingpongFBO[1] = 0;
    name_ = "Bloom";
}

BloomEffect::~BloomEffect() {
    glDeleteFramebuffers(1, &_brightPassFBO);
    glDeleteTextures(1, &_brightPassTexture);
    glDeleteFramebuffers(2, _pingpongFBO);
    glDeleteTextures(2, _pingpongTexture);
}

void BloomEffect::Initialize(int width, int height) {
    _width = width;
    _height = height;

    _brightPassShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bright_pass.frag");
    _blurShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/gaussian_blur.frag");
    _compositeShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bloom_composite.frag");

    InitializeFBOs();
}

void BloomEffect::InitializeFBOs() {
    // Clean up existing resources
    glDeleteFramebuffers(1, &_brightPassFBO);
    glDeleteTextures(1, &_brightPassTexture);
    glDeleteFramebuffers(2, _pingpongFBO);
    glDeleteTextures(2, _pingpongTexture);

    // Bright pass FBO
    glGenFramebuffers(1, &_brightPassFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
    glGenTextures(1, &_brightPassTexture);
    glBindTexture(GL_TEXTURE_2D, _brightPassTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width, _height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _brightPassTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        logger::ERROR("Bloom Bright Pass FBO is not complete!");

    // Ping-pong FBOs for blurring
    glGenFramebuffers(2, _pingpongFBO);
    glGenTextures(2, _pingpongTexture);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, _pingpongTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width, _height, 0, GL_RGB, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _pingpongTexture[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            logger::ERROR("Bloom Ping-Pong FBO " + std::to_string(i) + " is not complete!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomEffect::Apply(GLuint sourceTexture) {
    GLint last_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);

    // 1. Bright pass: render scene to brightPassFBO
    glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _brightPassShader->use();
    _brightPassShader->setInt("sceneTexture", 0);
    _brightPassShader->setFloat("threshold", threshold_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Gaussian blur (ping-pong)
    bool horizontal = true, first_iteration = true;
    _blurShader->use();
    _blurShader->setInt("image", 0);
    for (unsigned int i = 0; i < _blurAmount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[horizontal]);
        _blurShader->setBool("horizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? _brightPassTexture : _pingpongTexture[!horizontal]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration)
            first_iteration = false;
    }

    // The final blurred texture is in _pingpongTexture[!horizontal]

    // 3. Composite original scene and blurred image
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
    _compositeShader->use();
    _compositeShader->setInt("sceneTexture", 0);
    _compositeShader->setInt("bloomBlur", 1);
    _compositeShader->setFloat("intensity", intensity_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _pingpongTexture[!horizontal]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind textures from texture units
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

void BloomEffect::Resize(int width, int height) {
    _width = width;
    _height = height;
    InitializeFBOs();
}

} // namespace PostProcessing
} // namespace Boidsish
