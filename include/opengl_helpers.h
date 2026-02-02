#pragma once

#include <GL/glew.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <numeric>
#include <string>
#include "logger.h"

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
    template <typename T, size_t Capacity = 1>
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

            fences_.assign(buffering_count_, nullptr);
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
                logger::INFO("PersistentRingBuffer::EnsureCapacity increasing to " + std::to_string(required_count * 2) + " for target " + std::to_string(target_));
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
            if (!ptr_ || fences_.empty()) return nullptr;

            // Wait for GPU to finish with this frame's part of the buffer
            if (fences_[current_frame_]) {
                GLenum wait_res = glClientWaitSync(fences_[current_frame_], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000); // 1s timeout
                if (wait_res == GL_WAIT_FAILED || wait_res == GL_TIMEOUT_EXPIRED) {
                    logger::ERROR("PersistentRingBuffer sync wait failed or timed out for target " + std::to_string(target_));
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
            if (vbo_ != 0 && size_per_frame_ > 0) {
                glBindBufferRange(target_, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
            }
        }

        /**
         * @brief Binds the current frame's range to a specific target and binding point.
         */
        void BindRange(GLenum target, GLuint binding) {
            if (vbo_ != 0 && size_per_frame_ > 0) {
                glBindBufferRange(target, binding, vbo_, current_frame_ * size_per_frame_, size_per_frame_);
            }
        }

        /**
         * @brief Advance to the next frame in the ring, placing a fence for the current one.
         */
        void AdvanceFrame() {
            if (!ptr_ || fences_.empty()) return;

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

            size_t alignment = 256;
            // Use lcm to find the smallest common multiple of alignment and sizeof(T)
            size_t unit_alignment = std::lcm(sizeof(T), alignment);
            size_per_frame_ = (raw_size + unit_alignment - 1) / unit_alignment * unit_alignment;
        }

        void Init() {
            total_size_ = size_per_frame_ * buffering_count_;
            if (total_size_ == 0) {
                return;
            }

            // Flags for glBufferStorage and glMapBufferRange
            // Including GL_CLIENT_STORAGE_BIT may help some drivers optimize persistent mapping
            const GLbitfield storage_flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT |
                                           GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT;
            const GLbitfield map_flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

            glGenBuffers(1, &vbo_);
            if (vbo_ == 0) {
                logger::ERROR("PersistentRingBuffer: glGenBuffers failed for target " + std::to_string(target_));
                return;
            }

            glBindBuffer(target_, vbo_);
            logger::INFO("PersistentRingBuffer::Init target=" + std::to_string(target_) +
                         " size=" + std::to_string(total_size_) +
                         " count=" + std::to_string(count_) +
                         " storage_flags=" + std::to_string(storage_flags));
            glBufferStorage(target_, total_size_, nullptr, storage_flags);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                logger::ERROR("PersistentRingBuffer: glBufferStorage failed for target " + std::to_string(target_) + " with error " + std::to_string(err));
                glDeleteBuffers(1, &vbo_);
                vbo_ = 0;
                return;
            }

            ptr_ = glMapBufferRange(target_, 0, total_size_, map_flags);
            if (!ptr_) {
                logger::ERROR("PersistentRingBuffer: glMapBufferRange failed for target " + std::to_string(target_));
                glDeleteBuffers(1, &vbo_);
                vbo_ = 0;
                return;
            }

            // fences_ already assigned in constructor or Cleanup()
            current_frame_ = 0;
            logger::INFO("PersistentRingBuffer initialized for target " + std::to_string(target_) + ". VBO=" + std::to_string(vbo_) + ", total_size=" + std::to_string(total_size_) + ", count=" + std::to_string(count_));
        }

        void Cleanup() {
            if (vbo_ != 0) {
                glBindBuffer(target_, vbo_);
                if (ptr_) {
                    glUnmapBuffer(target_);
                    ptr_ = nullptr;
                }
                glDeleteBuffers(1, &vbo_);
                vbo_ = 0;
            }
            for (auto fence : fences_) {
                if (fence) glDeleteSync(fence);
            }
            fences_.assign(buffering_count_, nullptr);
            logger::INFO("PersistentRingBuffer cleaned up for target " + std::to_string(target_));
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
