#pragma once

#include <GL/glew.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace Boidsish {

    /**
     * @brief Helper for approached-zero driver overhead (AZDO) buffer management.
     * Uses persistent mapped triple buffering with fences.
     */
    class PersistentRingBuffer {
    public:
        PersistentRingBuffer(GLenum target, size_t initial_size_per_frame, int buffering_count = 3)
            : target_(target), size_per_frame_(initial_size_per_frame), buffering_count_(buffering_count) {

            size_per_frame_ = (size_per_frame_ + 255) & ~255;
            Init();
        }

        ~PersistentRingBuffer() {
            Cleanup();
        }

        void EnsureCapacity(size_t required_size_per_frame) {
            if (required_size_per_frame > size_per_frame_) {
                Cleanup();
                size_per_frame_ = (required_size_per_frame * 2 + 255) & ~255;
                Init();
            }
        }

        void* GetCurrentPtr() {
            if (!ptr_) return nullptr;

            // Wait for GPU to finish with this frame's part of the buffer
            if (fences_[current_frame_]) {
                GLenum wait_res = glClientWaitSync(fences_[current_frame_], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000); // 1s timeout
                if (wait_res == GL_WAIT_FAILED || wait_res == GL_TIMEOUT_EXPIRED) {
                    // std::cerr << "[OpenGL] Sync wait failed or timed out!" << std::endl;
                }
                glDeleteSync(fences_[current_frame_]);
                fences_[current_frame_] = nullptr;
            }

            return static_cast<uint8_t*>(ptr_) + (current_frame_ * size_per_frame_);
        }

        void BindRange(GLuint binding) {
            glBindBufferRange(target_, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
        }

        void BindRange(GLenum target, GLuint binding) {
             glBindBufferRange(target, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
        }

        void AdvanceFrame() {
            // Lock this range
            if (fences_[current_frame_]) glDeleteSync(fences_[current_frame_]);
            fences_[current_frame_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

            current_frame_ = (current_frame_ + 1) % buffering_count_;
        }

        GLuint GetVBO() const { return vbo_; }
        size_t GetOffset() const { return current_frame_ * size_per_frame_; }
        size_t GetSizePerFrame() const { return size_per_frame_; }

    private:
        void Init() {
            total_size_ = size_per_frame_ * buffering_count_;
            const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

            glGenBuffers(1, &vbo_);
            glBindBuffer(target_, vbo_);
            glBufferStorage(target_, total_size_, nullptr, flags);

            ptr_ = glMapBufferRange(target_, 0, total_size_, flags);
            fences_.assign(buffering_count_, nullptr);
            current_frame_ = 0;
        }

        void Cleanup() {
            if (vbo_) {
                glBindBuffer(target_, vbo_);
                if (ptr_) glUnmapBuffer(target_);
                glDeleteBuffers(1, &vbo_);
                vbo_ = 0;
                ptr_ = nullptr;
            }
            for (auto fence : fences_) {
                if (fence) glDeleteSync(fence);
            }
            fences_.clear();
        }

        GLenum target_;
        GLuint vbo_ = 0;
        void* ptr_ = nullptr;
        size_t size_per_frame_;
        size_t total_size_;
        int buffering_count_;
        int current_frame_ = 0;
        std::vector<GLsync> fences_;
    };

}
