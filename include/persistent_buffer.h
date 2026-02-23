#pragma once

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <GL/glew.h>

namespace Boidsish {

	/**
	 * @brief A persistent mapped buffer for AZDO (Approaching Zero Driver Overhead) rendering.
	 *
	 * This class uses glBufferStorage with GL_MAP_PERSISTENT_BIT and GL_MAP_COHERENT_BIT
	 * to provide a buffer that is permanently mapped to CPU memory.
	 * It uses triple-buffering to avoid CPU-GPU synchronization stalls.
	 */
	template <typename T>
	class PersistentBuffer {
	public:
		PersistentBuffer(GLenum target, size_t element_count, int num_buffers = 3):
			target_(target), element_count_(element_count), num_buffers_(num_buffers) {
			size_t total_size = element_count_ * sizeof(T) * num_buffers_;

			glGenBuffers(1, &buffer_id_);
			glBindBuffer(target_, buffer_id_);

			GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
			// Also add GL_DYNAMIC_STORAGE_BIT to allow for standard updates if needed,
			// though not strictly required for persistent mapping.
			glBufferStorage(target_, total_size, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);

			mapped_data_ = static_cast<T*>(glMapBufferRange(target_, 0, total_size, flags));

			glBindBuffer(target_, 0);

			if (!mapped_data_) {
				glDeleteBuffers(1, &buffer_id_);
				buffer_id_ = 0;
				throw std::runtime_error(
					"Failed to map persistent buffer - GPU memory exhausted or invalid parameters"
				);
			}
		}

		~PersistentBuffer() {
			if (buffer_id_) {
				glBindBuffer(target_, buffer_id_);
				glUnmapBuffer(target_);
				glDeleteBuffers(1, &buffer_id_);
			}
		}

		// Non-copyable
		PersistentBuffer(const PersistentBuffer&) = delete;
		PersistentBuffer& operator=(const PersistentBuffer&) = delete;

		/**
		 * @brief Get a pointer to the current frame's buffer segment.
		 * @note Asserts that the buffer was successfully mapped.
		 */
		T* GetFrameDataPtr() {
			assert(mapped_data_ != nullptr && "PersistentBuffer was not successfully mapped");
			return mapped_data_ + (current_buffer_index_ * element_count_);
		}

		/**
		 * @brief Get the byte offset for the current frame's buffer segment.
		 */
		size_t GetFrameOffset() const { return current_buffer_index_ * element_count_ * sizeof(T); }

		/**
		 * @brief Advance to the next buffer segment (call once per frame).
		 */
		void AdvanceFrame() { current_buffer_index_ = (current_buffer_index_ + 1) % num_buffers_; }

		GLuint GetBufferId() const { return buffer_id_; }

		size_t GetElementCount() const { return element_count_; }

		size_t GetTotalSize() const { return element_count_ * sizeof(T) * num_buffers_; }

		int GetCurrentBufferIndex() const { return current_buffer_index_; }

		int GetNumBuffers() const { return num_buffers_; }

		T* GetFullBufferPtr() { return mapped_data_; }

	private:
		GLuint buffer_id_ = 0;
		GLenum target_;
		size_t element_count_;
		int    num_buffers_;
		int    current_buffer_index_ = 0;
		T*     mapped_data_ = nullptr;
	};

} // namespace Boidsish
