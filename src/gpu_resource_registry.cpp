#include <mutex>

#include "gpu_resource_registry.h"

namespace Boidsish {

void GpuResourceRegistry::PublishTexture(int slot, GLuint handle, GLenum target) {
	assert(slot >= 0 && slot < kMaxSlots);
	std::unique_lock lock(mtx_);
	textures_[slot] = {handle, target};
}

void GpuResourceRegistry::PublishUbo(int slot, GLuint handle) {
	assert(slot >= 0 && slot < kMaxSlots);
	std::unique_lock lock(mtx_);
	ubos_[slot] = handle;
}

void GpuResourceRegistry::PublishSsbo(int slot, GLuint handle) {
	assert(slot >= 0 && slot < kMaxSlots);
	std::unique_lock lock(mtx_);
	ssbos_[slot] = handle;
}

GLuint GpuResourceRegistry::GetTexture(int slot) const {
	assert(slot >= 0 && slot < kMaxSlots);
	std::shared_lock lock(mtx_);
	return textures_[slot].handle;
}

GLenum GpuResourceRegistry::GetTextureTarget(int slot) const {
	assert(slot >= 0 && slot < kMaxSlots);
	std::shared_lock lock(mtx_);
	return textures_[slot].target;
}

GLuint GpuResourceRegistry::GetUbo(int slot) const {
	assert(slot >= 0 && slot < kMaxSlots);
	std::shared_lock lock(mtx_);
	return ubos_[slot];
}

GLuint GpuResourceRegistry::GetSsbo(int slot) const {
	assert(slot >= 0 && slot < kMaxSlots);
	std::shared_lock lock(mtx_);
	return ssbos_[slot];
}

void GpuResourceRegistry::BindTextures(std::initializer_list<int> slots) const {
	std::shared_lock lock(mtx_);
	for (int slot : slots) {
		assert(slot >= 0 && slot < kMaxSlots);
		const auto& entry = textures_[slot];
		if (entry.handle != 0) {
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(entry.target, entry.handle);
		}
	}
}

} // namespace Boidsish
