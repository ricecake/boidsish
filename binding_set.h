#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include <GL/glew.h>

namespace Boidsish {

inline constexpr int kMaxTextureBindings = 16;
inline constexpr int kMaxBufferBindings = 8;

struct TextureBinding {
	int    unit = -1;
	GLuint texture = 0;
	GLenum target = GL_TEXTURE_2D;
};

struct BufferBinding {
	GLenum target = GL_SHADER_STORAGE_BUFFER;
	int    binding_point = -1;
	GLuint buffer = 0;
	bool   use_range = false;
	GLintptr offset = 0;
	GLsizeiptr size = 0;
};

class BindingSet {
public:
	BindingSet() = default;

	BindingSet& Texture(int unit, GLuint tex, GLenum target = GL_TEXTURE_2D) {
		assert(num_textures_ < kMaxTextureBindings && "BindingSet texture capacity exceeded");
		textures_[num_textures_++] = {unit, tex, target};
		return *this;
	}

	BindingSet& Ssbo(int binding_point, GLuint buffer) {
		assert(num_buffers_ < kMaxBufferBindings && "BindingSet buffer capacity exceeded");
		buffers_[num_buffers_++] = {GL_SHADER_STORAGE_BUFFER, binding_point, buffer, false, 0, 0};
		return *this;
	}

	BindingSet& SsboRange(int binding_point, GLuint buffer, GLintptr offset, GLsizeiptr size) {
		assert(num_buffers_ < kMaxBufferBindings && "BindingSet buffer capacity exceeded");
		buffers_[num_buffers_++] = {GL_SHADER_STORAGE_BUFFER, binding_point, buffer, true, offset, size};
		return *this;
	}

	BindingSet& Ubo(int binding_point, GLuint buffer) {
		assert(num_buffers_ < kMaxBufferBindings && "BindingSet buffer capacity exceeded");
		buffers_[num_buffers_++] = {GL_UNIFORM_BUFFER, binding_point, buffer, false, 0, 0};
		return *this;
	}

	void Apply() const {
#ifndef NDEBUG
		Validate();
#endif
		for (int i = 0; i < num_textures_; ++i) {
			const auto& t = textures_[i];
			glActiveTexture(GL_TEXTURE0 + t.unit);
			glBindTexture(t.target, t.texture);
		}
		for (int i = 0; i < num_buffers_; ++i) {
			const auto& b = buffers_[i];
			if (b.use_range) {
				glBindBufferRange(b.target, b.binding_point, b.buffer, b.offset, b.size);
			} else {
				glBindBufferBase(b.target, b.binding_point, b.buffer);
			}
		}
	}

	void Unbind() const {
		for (int i = 0; i < num_textures_; ++i) {
			const auto& t = textures_[i];
			glActiveTexture(GL_TEXTURE0 + t.unit);
			glBindTexture(t.target, 0);
		}
		for (int i = 0; i < num_buffers_; ++i) {
			const auto& b = buffers_[i];
			glBindBufferBase(b.target, b.binding_point, 0);
		}
	}

	// Check for conflicts within this set. Fires assert on failure.
	void Validate() const {
		for (int i = 0; i < num_textures_; ++i) {
			for (int j = i + 1; j < num_textures_; ++j) {
				if (textures_[i].unit == textures_[j].unit) {
					std::cerr << "BindingSet conflict: texture unit " << textures_[i].unit
							  << " bound twice (textures " << textures_[i].texture
							  << " and " << textures_[j].texture << ")" << std::endl;
					assert(false && "Duplicate texture unit in BindingSet");
				}
			}
		}
		for (int i = 0; i < num_buffers_; ++i) {
			for (int j = i + 1; j < num_buffers_; ++j) {
				if (buffers_[i].target == buffers_[j].target &&
				    buffers_[i].binding_point == buffers_[j].binding_point) {
					const char* type = buffers_[i].target == GL_UNIFORM_BUFFER ? "UBO" : "SSBO";
					std::cerr << "BindingSet conflict: " << type << " binding point "
							  << buffers_[i].binding_point << " bound twice (buffers "
							  << buffers_[i].buffer << " and " << buffers_[j].buffer << ")"
							  << std::endl;
					assert(false && "Duplicate buffer binding point in BindingSet");
				}
			}
		}
	}

	// Check for conflicts between two BindingSets. Returns true if conflicts exist.
	// Prints diagnostics to stderr.
	static bool CheckConflicts(const BindingSet& a, const BindingSet& b) {
		bool has_conflict = false;

		for (int i = 0; i < a.num_textures_; ++i) {
			for (int j = 0; j < b.num_textures_; ++j) {
				if (a.textures_[i].unit == b.textures_[j].unit) {
					std::cerr << "BindingSet cross-conflict: texture unit " << a.textures_[i].unit
							  << " used by both sets (textures " << a.textures_[i].texture
							  << " and " << b.textures_[j].texture << ")" << std::endl;
					has_conflict = true;
				}
			}
		}

		for (int i = 0; i < a.num_buffers_; ++i) {
			for (int j = 0; j < b.num_buffers_; ++j) {
				if (a.buffers_[i].target == b.buffers_[j].target &&
				    a.buffers_[i].binding_point == b.buffers_[j].binding_point) {
					const char* type = a.buffers_[i].target == GL_UNIFORM_BUFFER ? "UBO" : "SSBO";
					std::cerr << "BindingSet cross-conflict: " << type << " binding point "
							  << a.buffers_[i].binding_point << " used by both sets (buffers "
							  << a.buffers_[i].buffer << " and " << b.buffers_[j].buffer << ")"
							  << std::endl;
					has_conflict = true;
				}
			}
		}

		return has_conflict;
	}

	int GetTextureCount() const { return num_textures_; }
	int GetBufferCount() const { return num_buffers_; }

private:
	std::array<TextureBinding, kMaxTextureBindings> textures_{};
	std::array<BufferBinding, kMaxBufferBindings> buffers_{};
	int num_textures_ = 0;
	int num_buffers_ = 0;
};

} // namespace Boidsish
