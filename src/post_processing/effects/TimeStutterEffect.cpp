#include "post_processing/effects/TimeStutterEffect.h"

#include "shader.h"
#include <GL/glew.h>
#include "logger.h"
#include <random>

namespace Boidsish {
namespace PostProcessing {

namespace {
    constexpr float kStutterInterval = 0.25f;
    constexpr float kStutterDuration = 0.1f;
}

TimeStutterEffect::TimeStutterEffect(): rng_(std::random_device{}()) {
    name_ = "Time Stutter";
}

TimeStutterEffect::~TimeStutterEffect() {
    if (!history_fbo_.empty()) {
        glDeleteFramebuffers(history_fbo_.size(), history_fbo_.data());
        glDeleteTextures(history_texture_.size(), history_texture_.data());
    }
}

void TimeStutterEffect::Initialize(int width, int height) {
    blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
    width_ = width;
    height_ = height;
    InitializeFBOs(width, height);
}

void TimeStutterEffect::InitializeFBOs(int width, int height) {
    if (!history_fbo_.empty()) {
        glDeleteFramebuffers(history_fbo_.size(), history_fbo_.data());
        glDeleteTextures(history_texture_.size(), history_texture_.data());
    }

    history_fbo_.resize(kFrameHistoryCount);
    history_texture_.resize(kFrameHistoryCount);
    glGenFramebuffers(kFrameHistoryCount, history_fbo_.data());
    glGenTextures(kFrameHistoryCount, history_texture_.data());

    for (int i = 0; i < kFrameHistoryCount; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[i]);
        glBindTexture(GL_TEXTURE_2D, history_texture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history_texture_[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            logger::ERROR("TimeStutterEffect FBO not complete!");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TimeStutterEffect::Apply(GLuint sourceTexture) {
    // 1. Capture the current frame into our history buffer
    current_frame_idx_ = (current_frame_idx_ + 1) % kFrameHistoryCount;
    glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[current_frame_idx_]);

    blit_shader_->use();
    blit_shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Decide whether to stutter
    if (time_ > stutter_end_time_) { // Not currently stuttering
        displayed_frame_offset_ = 0; // Show live frame
        if (time_ - last_stutter_time_ > kStutterInterval) {
            // Trigger a new stutter
            std::uniform_int_distribution<int> dist(1, kFrameHistoryCount - 1);
            displayed_frame_offset_ = dist(rng_);
            last_stutter_time_ = time_;
            stutter_end_time_ = time_ + kStutterDuration;
        }
    }
    // If we are currently stuttering, displayed_frame_offset_ remains set from when the stutter was triggered.

    // 3. Blit the chosen frame (either current or historical) to the output
    int frame_to_display_idx = (current_frame_idx_ - displayed_frame_offset_ + kFrameHistoryCount) % kFrameHistoryCount;

    blit_shader_->use();
    blit_shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, history_texture_[frame_to_display_idx]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void TimeStutterEffect::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    InitializeFBOs(width, height);
}

} // namespace PostProcessing
} // namespace Boidsish
