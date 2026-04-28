#pragma once

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <GL/glew.h>

namespace Boidsish {

	/**
	 * @brief A type-safe RAII wrapper for OpenGL textures.
	 *
	 * This class manages the lifecycle of an OpenGL texture handle and
	 * encourages the use of immutable storage (glTexStorage) for better
	 * performance and predictability.
	 */
	class PersistentTexture {
	public:
		PersistentTexture(GLenum target, GLint internal_format, int width, int height, int depth = 1, int levels = 1):
			target_(target), internal_format_(internal_format), width_(width), height_(height), depth_(depth), levels_(levels) {

			glGenTextures(1, &texture_id_);
			glBindTexture(target_, texture_id_);

			if (target_ == GL_TEXTURE_3D || target_ == GL_TEXTURE_2D_ARRAY) {
				glTexStorage3D(target_, levels_, internal_format_, width_, height_, depth_);
			} else {
				glTexStorage2D(target_, levels_, internal_format_, width_, height_);
			}

			// Default parameters
			if (target_ == GL_TEXTURE_3D) {
				glTexParameteri(target_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}
			glTexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glBindTexture(target_, 0);

			if (texture_id_ == 0) {
				throw std::runtime_error("Failed to create OpenGL texture");
			}
		}

		~PersistentTexture() {
			if (texture_id_) {
				glDeleteTextures(1, &texture_id_);
			}
		}

		// Non-copyable
		PersistentTexture(const PersistentTexture&) = delete;
		PersistentTexture& operator=(const PersistentTexture&) = delete;

		// Moveable
		PersistentTexture(PersistentTexture&& other) noexcept :
			texture_id_(other.texture_id_),
			target_(other.target_),
			internal_format_(other.internal_format_),
			width_(other.width_),
			height_(other.height_),
			depth_(other.depth_),
			levels_(other.levels_) {
			other.texture_id_ = 0;
		}

		PersistentTexture& operator=(PersistentTexture&& other) noexcept {
			if (this != &other) {
				if (texture_id_) {
					glDeleteTextures(1, &texture_id_);
				}
				texture_id_ = other.texture_id_;
				target_ = other.target_;
				internal_format_ = other.internal_format_;
				width_ = other.width_;
				height_ = other.height_;
				depth_ = other.depth_;
				levels_ = other.levels_;
				other.texture_id_ = 0;
			}
			return *this;
		}

		/**
		 * @brief Bind the texture to a specific texture unit.
		 * @param unit The texture unit index (0, 1, 2, ...).
		 */
		void Bind(int unit) const {
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(target_, texture_id_);
		}

		/**
		 * @brief Unbind the texture from its target.
		 * @param unit The texture unit index.
		 */
		void Unbind(int unit) const {
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(target_, 0);
		}

		/**
		 * @brief Bind the texture as an image for compute shaders.
		 */
		void BindImage(GLuint unit, GLenum access, GLint level = 0, GLboolean layered = GL_FALSE, GLint layer = 0) const {
			glBindImageTexture(unit, texture_id_, level, layered, layer, access, internal_format_);
		}

		GLuint GetId() const { return texture_id_; }
		GLenum GetTarget() const { return target_; }
		int GetWidth() const { return width_; }
		int GetHeight() const { return height_; }
		int GetDepth() const { return depth_; }
		int GetLevels() const { return levels_; }
		GLint GetInternalFormat() const { return internal_format_; }

		/**
		 * @brief Update a region of a 2D texture.
		 */
		void Update2D(const void* data, int xoffset, int yoffset, int width, int height, GLenum format, GLenum type, int level = 0) {
			assert(target_ != GL_TEXTURE_3D && target_ != GL_TEXTURE_2D_ARRAY);
			glBindTexture(target_, texture_id_);
			glTexSubImage2D(target_, level, xoffset, yoffset, width, height, format, type, data);
		}

		/**
		 * @brief Update a region of a 3D or 2D array texture.
		 */
		void Update3D(const void* data, int xoffset, int yoffset, int zoffset, int width, int height, int depth, GLenum format, GLenum type, int level = 0) {
			assert(target_ == GL_TEXTURE_3D || target_ == GL_TEXTURE_2D_ARRAY);
			glBindTexture(target_, texture_id_);
			glTexSubImage3D(target_, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
		}

		/**
		 * @brief Set texture parameters.
		 */
		void SetParameter(GLenum pname, GLint param) {
			glBindTexture(target_, texture_id_);
			glTexParameteri(target_, pname, param);
		}

		void SetParameter(GLenum pname, const GLfloat* params) {
			glBindTexture(target_, texture_id_);
			glTexParameterfv(target_, pname, params);
		}

	private:
		GLuint texture_id_ = 0;
		GLenum target_;
		GLint  internal_format_;
		int    width_;
		int    height_;
		int    depth_;
		int    levels_;
	};

} // namespace Boidsish
