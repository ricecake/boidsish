#pragma once

#include <algorithm>
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
	 *
	 * For GL_UNIFORM_BUFFER targets, per-frame segments are padded to meet
	 * GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT so glBindBufferRange offsets are valid.
	 */
	template <typename T>
	class PersistentBuffer {
	public:
		PersistentBuffer(
			GLenum     target,
			size_t     element_count,
			int        num_buffers = 3,
			GLbitfield map_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
		):
			target_(target), element_count_(element_count), num_buffers_(num_buffers) {

			// Compute per-frame stride in bytes, aligned for all targets.
			// 256 bytes is a common maximum alignment requirement for UBOs and SSBOs
			// across different hardware, ensuring glBindBufferRange offsets are always valid.
			size_t raw_frame_bytes = element_count_ * sizeof(T);
			GLint  alignment = 256;
			if (target_ == GL_UNIFORM_BUFFER) {
				glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
			} else if (target_ == GL_SHADER_STORAGE_BUFFER) {
				glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);
			} else if (target_ == GL_DRAW_INDIRECT_BUFFER || target_ == GL_DISPATCH_INDIRECT_BUFFER) {
				// Indirect buffers must be 16-byte aligned for many drivers/APIs
				alignment = 16;
			}

			// Ensure a minimum alignment of 16 for all triple-buffered segments to be safe
			alignment = std::max(alignment, 16);
			aligned_frame_stride_ =
				((raw_frame_bytes + (size_t)alignment - 1) / (size_t)alignment) * (size_t)alignment;

			size_t total_size = aligned_frame_stride_ * num_buffers_;

			glGenBuffers(1, &buffer_id_);
			glBindBuffer(target_, buffer_id_);

			// Also add GL_DYNAMIC_STORAGE_BIT to allow for standard updates if needed,
			// though not strictly required for persistent mapping.
			glBufferStorage(target_, total_size, nullptr, map_flags | GL_DYNAMIC_STORAGE_BIT);

			mapped_bytes_ = static_cast<char*>(glMapBufferRange(target_, 0, total_size, map_flags));

			glBindBuffer(target_, 0);

			if (!mapped_bytes_) {
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
			assert(mapped_bytes_ != nullptr && "PersistentBuffer was not successfully mapped");
			return reinterpret_cast<T*>(mapped_bytes_ + (current_buffer_index_ * aligned_frame_stride_));
		}

		/**
		 * @brief Get a pointer to a specific frame's buffer segment.
		 */
		T* GetFrameDataPtr(int buffer_index) {
			assert(mapped_bytes_ != nullptr && "PersistentBuffer was not successfully mapped");
			assert(buffer_index >= 0 && buffer_index < num_buffers_);
			return reinterpret_cast<T*>(mapped_bytes_ + (buffer_index * aligned_frame_stride_));
		}

		/**
		 * @brief Get the byte offset for the current frame's buffer segment.
		 * Guaranteed to be aligned for GL_UNIFORM_BUFFER targets.
		 */
		size_t GetFrameOffset() const { return current_buffer_index_ * aligned_frame_stride_; }

		/**
		 * @brief Advance to the next buffer segment (call once per frame).
		 */
		void AdvanceFrame() { current_buffer_index_ = (current_buffer_index_ + 1) % num_buffers_; }

		GLuint GetBufferId() const { return buffer_id_; }

		size_t GetElementCount() const { return element_count_; }

		size_t GetTotalSize() const { return aligned_frame_stride_ * num_buffers_; }

		int GetCurrentBufferIndex() const { return current_buffer_index_; }

		int GetNumBuffers() const { return num_buffers_; }

		T* GetFullBufferPtr() { return reinterpret_cast<T*>(mapped_bytes_); }

		/**
		 * @brief Bind the buffer to a specific binding point using the full buffer.
		 * @param index The binding point index.
		 */
		void BindBase(GLuint index) const { glBindBufferBase(target_, index, buffer_id_); }

		/**
		 * @brief Bind the current frame's segment to a specific binding point.
		 * @param index The binding point index.
		 */
		void BindRange(GLuint index) const {
			glBindBufferRange(target_, index, buffer_id_, GetFrameOffset(), aligned_frame_stride_);
		}

	private:
		GLuint buffer_id_ = 0;
		GLenum target_;
		size_t element_count_;
		int    num_buffers_;
		int    current_buffer_index_ = 0;
		char*  mapped_bytes_ = nullptr;
		size_t aligned_frame_stride_ = 0;
	};

} // namespace Boidsish
