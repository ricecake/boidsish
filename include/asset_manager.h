#pragma once

#include <map>
#include <memory>
#include <set>
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
		 * @brief Get the bindless handle for a texture.
		 * If bindless textures are not supported, returns 0.
		 */
		uint64_t GetTextureHandle(GLuint texture_id);

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

	private:
		AssetManager();
		~AssetManager();

		bool m_bindless_supported = false;

		std::map<std::string, std::shared_ptr<ModelData>>                       m_models;
		std::map<std::string, GLuint>                                           m_textures;
		std::set<uint64_t>                                                      m_resident_handles;
		std::map<std::string, std::shared_ptr<ma_resource_manager_data_source>> m_audio_sources;
	};

} // namespace Boidsish
