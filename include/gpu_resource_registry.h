#pragma once

#include <array>
#include <cassert>
#include <initializer_list>
#include <shared_mutex>

#include <GL/glew.h>

namespace Boidsish {

class GpuResourceRegistry {
public:
	void PublishTexture(int slot, GLuint handle, GLenum target = GL_TEXTURE_2D);
	void PublishUbo(int slot, GLuint handle);
	void PublishSsbo(int slot, GLuint handle);

	GLuint GetTexture(int slot) const;
	GLenum GetTextureTarget(int slot) const;
	GLuint GetUbo(int slot) const;
	GLuint GetSsbo(int slot) const;

	void BindTextures(std::initializer_list<int> slots) const;

	static GpuResourceRegistry& Instance() {
		assert(s_instance && "GpuResourceRegistry not yet initialized");
		return *s_instance;
	}

	static void SetInstance(GpuResourceRegistry* reg) { s_instance = reg; }

private:
	static constexpr int kMaxSlots = 64;
	static inline GpuResourceRegistry* s_instance = nullptr;

	struct TextureEntry {
		GLuint handle = 0;
		GLenum target = GL_TEXTURE_2D;
	};

	std::array<TextureEntry, kMaxSlots> textures_{};
	std::array<GLuint, kMaxSlots>       ubos_{};
	std::array<GLuint, kMaxSlots>       ssbos_{};

	mutable std::shared_mutex mtx_;
};

} // namespace Boidsish
