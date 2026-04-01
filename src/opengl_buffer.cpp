#include "opengl_buffer.h"

#include <utility>

#include <GLFW/glfw3.h>

namespace Boidsish {

	// --- Buffer Implementation ---

	Buffer::Buffer(GLenum target): target_(target) {
		if (glfwGetCurrentContext() != nullptr) {
			glGenBuffers(1, &id_);
		}
	}

	Buffer::~Buffer() {
		if (id_ != 0 && glfwGetCurrentContext() != nullptr) {
			glDeleteBuffers(1, &id_);
		}
	}

	Buffer::Buffer(Buffer&& other) noexcept: id_(other.id_), target_(other.target_) {
		other.id_ = 0;
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept {
		if (this != &other) {
			if (id_ != 0) {
				glDeleteBuffers(1, &id_);
			}
			id_ = other.id_;
			target_ = other.target_;
			other.id_ = 0;
		}
		return *this;
	}

	void Buffer::Bind() const {
		glBindBuffer(target_, id_);
	}

	void Buffer::Unbind() const {
		glBindBuffer(target_, 0);
	}

	// --- VertexArray Implementation ---

	VertexArray::VertexArray() {
		if (glfwGetCurrentContext() != nullptr) {
			glGenVertexArrays(1, &id_);
		}
	}

	VertexArray::~VertexArray() {
		if (id_ != 0 && glfwGetCurrentContext() != nullptr) {
			glDeleteVertexArrays(1, &id_);
		}
	}

	VertexArray::VertexArray(VertexArray&& other) noexcept: id_(other.id_) {
		other.id_ = 0;
	}

	VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
		if (this != &other) {
			if (id_ != 0) {
				glDeleteVertexArrays(1, &id_);
			}
			id_ = other.id_;
			other.id_ = 0;
		}
		return *this;
	}

	void VertexArray::Bind() const {
		glBindVertexArray(id_);
	}

	void VertexArray::Unbind() const {
		glBindVertexArray(0);
	}

} // namespace Boidsish
