#pragma once

#include <GL/glew.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <numeric>

namespace Boidsish {

    /**
     * @brief OpenGL indirect draw command structures.
     */
    struct DrawElementsIndirectCommand {
        uint32_t count;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t  baseVertex;
        uint32_t baseInstance;
    };

    struct DrawArraysIndirectCommand {
        uint32_t count;
        uint32_t instanceCount;
        uint32_t first;
        uint32_t baseInstance;
    };

    /**
     * @brief Helper for approached-zero driver overhead (AZDO) buffer management.
     * Uses persistent mapped triple buffering with fences.
     * Templated on type T and optional compile-time Capacity.
     */
    template <typename T, size_t Capacity = 0>
    class PersistentRingBuffer {
    public:
        /**
         * @brief Construct a new Persistent Ring Buffer
         * @param target OpenGL buffer target (e.g., GL_ARRAY_BUFFER, GL_UNIFORM_BUFFER)
         * @param count Number of elements of type T per frame. Defaults to Capacity template arg.
         * @param buffering_count Number of frames to buffer (triple buffering recommended).
         */
        PersistentRingBuffer(GLenum target, size_t count = Capacity, int buffering_count = 3)
            : target_(target), count_(count), buffering_count_(buffering_count) {

            CalculateStride();
            Init();
        }

        ~PersistentRingBuffer() {
            Cleanup();
        }

        /**
         * @brief Reallocates the buffer if the required count exceeds current capacity.
         */
        void EnsureCapacity(size_t required_count) {
            if (required_count > count_) {
                Cleanup();
                count_ = (required_count * 2); // Double for headroom
                CalculateStride();
                Init();
            }
        }

        /**
         * @brief Get a pointer to the current frame's memory region.
         * Blocks if the GPU is still using this specific region (triple buffering minimizes this).
         */
        T* GetCurrentPtr() {
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

            return reinterpret_cast<T*>(static_cast<uint8_t*>(ptr_) + (current_frame_ * size_per_frame_));
        }

        /**
         * @brief Binds the current frame's range to a binding point (for UBOs/SSBOs).
         */
        void BindRange(GLuint binding) {
            glBindBufferRange(target_, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
        }

        /**
         * @brief Binds the current frame's range to a specific target and binding point.
         */
        void BindRange(GLenum target, GLuint binding) {
             glBindBufferRange(target, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
        }

        /**
         * @brief Advance to the next frame in the ring, placing a fence for the current one.
         */
        void AdvanceFrame() {
            // Lock this range
            if (fences_[current_frame_]) glDeleteSync(fences_[current_frame_]);
            fences_[current_frame_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

            current_frame_ = (current_frame_ + 1) % buffering_count_;
        }

        GLuint GetVBO() const { return vbo_; }
        size_t GetOffset() const { return current_frame_ * size_per_frame_; }
        size_t GetSizePerFrame() const { return size_per_frame_; }
        size_t GetCount() const { return count_; }

    private:
        void CalculateStride() {
            size_t raw_size = count_ * sizeof(T);
            if (raw_size == 0) {
                size_per_frame_ = 0;
                return;
            }

            // size_per_frame must be:
            // 1. Multiple of 256 (for UBO alignment)
            // 2. Multiple of sizeof(T) (so vertex pointers remain aligned to element boundaries)

            if constexpr (sizeof(T) == 0) {
                size_per_frame_ = (raw_size + 255) & ~255;
            } else {
                size_t alignment = 256;
                // Use lcm to find the smallest common multiple of alignment and sizeof(T)
                size_t unit_alignment = std::lcm(sizeof(T), alignment);
                size_per_frame_ = (raw_size + unit_alignment - 1) / unit_alignment * unit_alignment;
            }
        }

        void Init() {
            total_size_ = size_per_frame_ * buffering_count_;
            if (total_size_ == 0) return;

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
        size_t count_ = 0;
        size_t size_per_frame_ = 0;
        size_t total_size_ = 0;
        int buffering_count_ = 3;
        int current_frame_ = 0;
        std::vector<GLsync> fences_;
    };

}
