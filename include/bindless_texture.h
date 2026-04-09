#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <GL/glew.h>
#include "handle.h"

namespace Boidsish {

	/**
	 * @brief Representation of a bindless texture.
	 */
	struct BindlessTexture {
		GLuint   textureId = 0;
		uint64_t handle = 0;
		bool     isResident = false;
	};

	using BindlessTextureHandle = Handle<BindlessTexture>;

	/**
	 * @brief Manages the lifecycle and residency of bindless textures.
	 */
	class BindlessTextureManager {
	public:
		static BindlessTextureManager& GetInstance();

		// Non-copyable
		BindlessTextureManager(const BindlessTextureManager&) = delete;
		BindlessTextureManager& operator=(const BindlessTextureManager&) = delete;

		/**
		 * @brief Get or create a bindless handle for an existing OpenGL texture.
		 * @param textureId The OpenGL texture ID.
		 * @return A handle to the bindless texture resource.
		 */
		BindlessTextureHandle GetBindlessHandle(GLuint textureId);

		/**
		 * @brief Make a bindless texture resident.
		 */
		void MakeResident(BindlessTextureHandle handle);

		/**
		 * @brief Make a bindless texture non-resident.
		 */
		void MakeNonResident(BindlessTextureHandle handle);

		/**
		 * @brief Get the 64-bit GPU handle for a given resource handle.
		 */
		uint64_t GetGpuHandle(BindlessTextureHandle handle) const;

		/**
		 * @brief Release bindless resources for a texture.
		 * Note: This does NOT delete the underlying OpenGL texture.
		 */
		void ReleaseHandle(BindlessTextureHandle handle);

		/**
		 * @brief Clear all bindless handles.
		 * Should be called before context destruction.
		 */
		void Clear();

	private:
		BindlessTextureManager() = default;
		~BindlessTextureManager();

		std::unordered_map<uint32_t, BindlessTexture> m_textures;
		uint32_t                                     m_nextId = 1;
		std::unordered_map<GLuint, uint32_t>         m_textureToHandle;
	};

} // namespace Boidsish
