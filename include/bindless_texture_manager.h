#pragma once

#include <GL/glew.h>
#include <unordered_map>
#include <mutex>
#include "logger.h"

namespace Boidsish {

    /**
     * @brief Manages GL_ARB_bindless_texture residency and handle mapping.
     */
    class BindlessTextureManager {
    public:
        static BindlessTextureManager& GetInstance() {
            static BindlessTextureManager instance;
            return instance;
        }

        /**
         * @brief Check if bindless textures are supported by the driver.
         */
        bool IsSupported() const { return m_supported; }

        /**
         * @brief Get or create a bindless handle for a texture and make it resident.
         * @param textureId The OpenGL texture name.
         * @return The 64-bit bindless handle, or 0 if not supported.
         */
        GLuint64 GetHandle(GLuint textureId) {
            if (!m_supported || textureId == 0) return 0;

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_handles.find(textureId);
            if (it != m_handles.end()) {
                return it->second;
            }

            GLuint64 handle = glGetTextureHandleARB(textureId);
            if (handle != 0) {
                glMakeTextureHandleResidentARB(handle);
                m_handles[textureId] = handle;
                // logger::DEBUG("Bindless handle created for texture {}: {}", textureId, handle);
            } else {
                logger::ERROR("Failed to create bindless handle for texture {}", textureId);
            }
            return handle;
        }

        /**
         * @brief Release residency for all managed handles.
         * Should be called before context destruction.
         */
        void Shutdown() {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto const& [id, handle] : m_handles) {
                glMakeTextureHandleNonResidentARB(handle);
            }
            m_handles.clear();
        }

    private:
        BindlessTextureManager() {
            m_supported = (glewIsSupported("GL_ARB_bindless_texture") == GL_TRUE);
            if (m_supported) {
                logger::LOG("Modern OpenGL: GL_ARB_bindless_texture is supported.");
            } else {
                logger::WARNING("Bindless textures not supported. Falling back to legacy binding.");
            }
        }

        bool m_supported = false;
        std::unordered_map<GLuint, GLuint64> m_handles;
        std::mutex m_mutex;
    };

} // namespace Boidsish
