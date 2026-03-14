#include "bindless_texture.h"
#include "logger.h"
#include <GL/glew.h>

namespace Boidsish {

	BindlessTextureManager& BindlessTextureManager::GetInstance() {
		static BindlessTextureManager instance;
		return instance;
	}

	BindlessTextureManager::~BindlessTextureManager() {
		Clear();
	}

	BindlessTextureHandle BindlessTextureManager::GetBindlessHandle(GLuint textureId) {
		if (textureId == 0) return BindlessTextureHandle(0);

		auto it = m_textureToHandle.find(textureId);
		if (it != m_textureToHandle.end()) {
			return BindlessTextureHandle(it->second);
		}

		uint64_t gpuHandle = glGetTextureHandleARB(textureId);
		if (gpuHandle == 0) {
			logger::ERROR("Failed to get bindless handle for texture {}", textureId);
			return BindlessTextureHandle(0);
		}

		uint32_t id = m_nextId++;
		BindlessTexture bt;
		bt.textureId = textureId;
		bt.handle = gpuHandle;
		bt.isResident = false;

		m_textures[id] = bt;
		m_textureToHandle[textureId] = id;

		logger::LOG("Created bindless handle for texture {}: 0x{:016x}", textureId, gpuHandle);
		return BindlessTextureHandle(id);
	}

	void BindlessTextureManager::MakeResident(BindlessTextureHandle handle) {
		auto it = m_textures.find(handle.id);
		if (it != m_textures.end() && !it->second.isResident) {
			glMakeTextureHandleResidentARB(it->second.handle);
			it->second.isResident = true;
		}
	}

	void BindlessTextureManager::MakeNonResident(BindlessTextureHandle handle) {
		auto it = m_textures.find(handle.id);
		if (it != m_textures.end() && it->second.isResident) {
			glMakeTextureHandleNonResidentARB(it->second.handle);
			it->second.isResident = false;
		}
	}

	uint64_t BindlessTextureManager::GetGpuHandle(BindlessTextureHandle handle) const {
		auto it = m_textures.find(handle.id);
		if (it != m_textures.end()) {
			return it->second.handle;
		}
		return 0;
	}

	void BindlessTextureManager::ReleaseHandle(BindlessTextureHandle handle) {
		auto it = m_textures.find(handle.id);
		if (it != m_textures.end()) {
			if (it->second.isResident) {
				glMakeTextureHandleNonResidentARB(it->second.handle);
			}
			m_textureToHandle.erase(it->second.textureId);
			m_textures.erase(it);
		}
	}

	void BindlessTextureManager::Clear() {
		for (auto& [id, bt] : m_textures) {
			if (bt.isResident) {
				glMakeTextureHandleNonResidentARB(bt.handle);
			}
		}
		m_textures.clear();
		m_textureToHandle.clear();
		m_nextId = 1;
	}

} // namespace Boidsish
