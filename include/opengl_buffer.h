#pragma once

#include <cstdint>
#include <vector>

#include <GL/glew.h>

namespace Boidsish {

	/**
	 * @brief Base class for all OpenGL buffer objects.
	 * Handles common RAII management of the buffer ID.
	 */
	class Buffer {
	public:
		explicit Buffer(GLenum target);
		virtual ~Buffer();

		// Non-copyable
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		// Moveable
		Buffer(Buffer&& other) noexcept;
		Buffer& operator=(Buffer&& other) noexcept;

		void Bind() const;
		void Unbind() const;

		GLuint GetID() const { return id_; }

		GLenum GetTarget() const { return target_; }

	protected:
		GLuint id_ = 0;
		GLenum target_;
	};

	/**
	 * @brief A templated buffer that manages CPU-side data and synchronized GPU-side storage.
	 * Implements dirty tracking and deferred synchronization.
	 */
	template <typename T>
	class TypedBuffer: public Buffer {
	public:
		TypedBuffer(GLenum target, GLenum usage = GL_STATIC_DRAW): Buffer(target), usage_(usage) {}

		/**
		 * @brief Set the data in the CPU-side buffer and marks it as dirty.
		 */
		void SetData(const std::vector<T>& data) {
			data_ = data;
			is_dirty_ = true;
		}

		/**
		 * @brief Set the data in the CPU-side buffer and marks it as dirty.
		 */
		void SetData(std::vector<T>&& data) {
			data_ = std::move(data);
			is_dirty_ = true;
		}

		/**
		 * @brief Access CPU-side data for modification. Marks buffer as dirty.
		 */
		std::vector<T>& GetData() {
			is_dirty_ = true;
			return data_;
		}

		const std::vector<T>& GetData() const { return data_; }

		/**
		 * @brief Synchronizes CPU-side data with the GPU if the buffer is dirty.
		 */
		void Sync() const {
			if (!is_dirty_)
				return;

			Bind();
			glBufferData(target_, data_.size() * sizeof(T), data_.data(), usage_);
			is_dirty_ = false;
		}

		bool IsDirty() const { return is_dirty_; }

		size_t Size() const { return data_.size(); }

		GLenum GetUsage() const { return usage_; }

	protected:
		std::vector<T> data_;
		GLenum         usage_;
		mutable bool   is_dirty_ = false;
	};

	/**
	 * @brief A Vertex Buffer Object (VBO).
	 */
	template <typename T>
	class VertexBuffer: public TypedBuffer<T> {
	public:
		explicit VertexBuffer(GLenum usage = GL_STATIC_DRAW): TypedBuffer<T>(GL_ARRAY_BUFFER, usage) {}
	};

	/**
	 * @brief An Index Buffer Object (EBO/IBO).
	 */
	template <typename T>
	class IndexBuffer: public TypedBuffer<T> {
	public:
		explicit IndexBuffer(GLenum usage = GL_STATIC_DRAW): TypedBuffer<T>(GL_ELEMENT_ARRAY_BUFFER, usage) {}
	};

	/**
	 * @brief A Uniform Buffer Object (UBO).
	 */
	template <typename T>
	class UniformBuffer: public TypedBuffer<T> {
	public:
		explicit UniformBuffer(GLenum usage = GL_DYNAMIC_DRAW): TypedBuffer<T>(GL_UNIFORM_BUFFER, usage) {}

		/**
		 * @brief Bind the uniform buffer to a specific binding point.
		 */
		void BindBase(GLuint index) const {
			this->Sync();
			glBindBufferBase(this->target_, index, this->id_);
		}
	};

	/**
	 * @brief A Shader Storage Buffer Object (SSBO).
	 */
	template <typename T>
	class ShaderStorageBuffer: public TypedBuffer<T> {
	public:
		explicit ShaderStorageBuffer(GLenum usage = GL_DYNAMIC_DRAW): TypedBuffer<T>(GL_SHADER_STORAGE_BUFFER, usage) {}

		/**
		 * @brief Bind the storage buffer to a specific binding point.
		 */
		void BindBase(GLuint index) const {
			this->Sync();
			glBindBufferBase(this->target_, index, this->id_);
		}
	};

	/**
	 * @brief An Indirect Buffer for Multi-Draw Indirect (MDI).
	 */
	template <typename T>
	class IndirectBuffer: public TypedBuffer<T> {
	public:
		explicit IndirectBuffer(GLenum usage = GL_DYNAMIC_DRAW): TypedBuffer<T>(GL_DRAW_INDIRECT_BUFFER, usage) {}
	};

	/**
	 * @brief Represents a Vertex Array Object (VAO).
	 * Manages vertex attributes and their binding to vertex/index buffers.
	 */
	class VertexArray {
	public:
		VertexArray();
		~VertexArray();

		// Non-copyable
		VertexArray(const VertexArray&) = delete;
		VertexArray& operator=(const VertexArray&) = delete;

		// Moveable
		VertexArray(VertexArray&& other) noexcept;
		VertexArray& operator=(VertexArray&& other) noexcept;

		void Bind() const;
		void Unbind() const;

		/**
		 * @brief Adds a vertex buffer to the VAO and configures its layout.
		 */
		template <typename T>
		void AddVertexBuffer(const VertexBuffer<T>& vbo, const std::vector<GLint>& attributes) {
			Bind();
			vbo.Bind();
			vbo.Sync();

			size_t offset = 0;
			for (GLuint i = 0; i < (GLuint)attributes.size(); ++i) {
				glEnableVertexAttribArray(i);
				glVertexAttribPointer(i, attributes[i], GL_FLOAT, GL_FALSE, sizeof(T), (void*)offset);
				offset += attributes[i] * sizeof(float);
			}
			Unbind();
		}

		/**
		 * @brief Adds an index buffer to the VAO.
		 */
		template <typename T>
		void AddIndexBuffer(const IndexBuffer<T>& ebo) {
			Bind();
			ebo.Bind();
			ebo.Sync();
			Unbind();
		}

		GLuint GetID() const { return id_; }

	private:
		GLuint id_ = 0;
	};

} // namespace Boidsish
