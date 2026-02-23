#pragma once
#include <GL/glew.h>
#include <stdexcept>
#include <vector>

namespace Boidsish {

/**
 * @brief A wrapper for OpenGL buffers using persistent mapping (AZDO technique).
 * This eliminates the need for glBufferSubData and reduces CPU-GPU synchronization overhead.
 */
template <typename T>
class PersistentBuffer {
public:
    PersistentBuffer(GLenum target, size_t count) : target_(target), count_(count) {
        _Allocate(count);
    }

    ~PersistentBuffer() {
        _Release();
    }

    // Disable copy
    PersistentBuffer(const PersistentBuffer&) = delete;
    PersistentBuffer& operator=(const PersistentBuffer&) = delete;

    // Enable move
    PersistentBuffer(PersistentBuffer&& other) noexcept :
        buffer_id_(other.buffer_id_), target_(other.target_), count_(other.count_), data_(other.data_) {
        other.buffer_id_ = 0;
        other.data_ = nullptr;
    }

    PersistentBuffer& operator=(PersistentBuffer&& other) noexcept {
        if (this != &other) {
            _Release();
            buffer_id_ = other.buffer_id_;
            target_ = other.target_;
            count_ = other.count_;
            data_ = other.data_;
            other.buffer_id_ = 0;
            other.data_ = nullptr;
        }
        return *this;
    }

    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }

    T* data() { return data_; }
    const T* data() const { return data_; }

    GLuint id() const { return buffer_id_; }
    size_t count() const { return count_; }

    void bind() const { glBindBuffer(target_, buffer_id_); }

    void bindBase(GLuint index) const {
        glBindBufferBase(target_, index, buffer_id_);
    }

    void bindRange(GLuint index, GLintptr offset, GLsizeiptr size) const {
        glBindBufferRange(target_, index, buffer_id_, offset, size);
    }

    /**
     * @brief Reallocates the buffer if the requested count is larger than current capacity.
     * Note: This is an expensive operation as it creates a new immutable buffer.
     */
    void ensureCapacity(size_t requested_count) {
        if (requested_count > count_) {
            _Release();
            _Allocate(requested_count);
        }
    }

private:
    void _Allocate(size_t count) {
        count_ = count;
        glGenBuffers(1, &buffer_id_);
        glBindBuffer(target_, buffer_id_);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

        glBufferStorage(target_, count_ * sizeof(T), nullptr, flags | GL_DYNAMIC_STORAGE_BIT);

        data_ = (T*)glMapBufferRange(target_, 0, count_ * sizeof(T), flags);
        if (!data_) {
            throw std::runtime_error("Failed to map persistent buffer");
        }
        glBindBuffer(target_, 0);
    }

    void _Release() {
        if (buffer_id_) {
            glBindBuffer(target_, buffer_id_);
            glUnmapBuffer(target_);
            glDeleteBuffers(1, &buffer_id_);
            buffer_id_ = 0;
            data_ = nullptr;
        }
    }

    GLuint buffer_id_ = 0;
    GLenum target_;
    size_t count_;
    T* data_ = nullptr;
};

} // namespace Boidsish
