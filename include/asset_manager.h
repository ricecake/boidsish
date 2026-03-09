#pragma once

#include <map>
#include <memory>
#include <string>

#include <GL/glew.h>

// Forward declarations
struct ma_engine;
struct ma_resource_manager_data_source;

namespace Boidsish {

	struct ModelData;

	/**
	 * @brief Central manager for caching assets like models, textures, and sounds.
	 * This ensures that resources are only loaded once from disk and shared across instances.
	 */
	class AssetManager {
	public:
		static AssetManager& GetInstance();

		// Non-copyable
		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;

		/**
		 * @brief Load or retrieve a cached model.
		 */
		std::shared_ptr<ModelData> GetModelData(const std::string& path);

		/**
		 * @brief Load or retrieve a cached texture.
		 * @param path File path to the texture
		 * @param directory Optional base directory for resolving relative paths
		 */
		GLuint GetTexture(const std::string& path, const std::string& directory = "");

		/**
		 * @brief Load or retrieve a cached audio data source.
		 * @param path File path to the audio file
		 * @param engine Pointer to the miniaudio engine (required for the resource manager)
		 */
		std::shared_ptr<ma_resource_manager_data_source> GetAudioDataSource(const std::string& path, ma_engine* engine);

		/**
		 * @brief Clear all cached assets. Should be called when the graphics context is destroyed.
		 */
		void Clear();

		/**
		 * @brief Check if bindless textures (GL_ARB_bindless_texture) are supported by the GPU.
		 */
		bool IsBindlessSupported() const { return m_bindless_supported; }

		/**
		 * @brief Get or create a bindless texture handle for the given texture ID.
		 * The handle is automatically made resident if support is available.
		 */
		GLuint64 GetTextureHandle(GLuint texture_id);

		/**
		 * @brief Initialize the asset manager, checking for extension support.
		 */
		void Initialize();

	private:
		AssetManager() = default;
		~AssetManager();

		bool m_bindless_supported = false;

		std::map<std::string, std::shared_ptr<ModelData>>                       m_models;
		std::map<std::string, GLuint>                                           m_textures;
		std::map<GLuint, GLuint64>                                              m_texture_handles;
		std::map<std::string, std::shared_ptr<ma_resource_manager_data_source>> m_audio_sources;
	};

} // namespace Boidsish
