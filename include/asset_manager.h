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
		 * @brief Initialize the asset manager and detect GPU features (e.g., Bindless Textures).
		 */
		void Initialize();

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
		 * @return 64-bit handle, or 0 if not supported or texture not found.
		 */
		uint64_t GetBindlessHandle(GLuint textureId);

		bool IsBindlessSupported() const { return m_bindless_supported; }

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
		AssetManager() = default;
		~AssetManager();

		std::map<std::string, std::shared_ptr<ModelData>>                       m_models;
		std::map<std::string, GLuint>                                           m_textures;
		std::map<GLuint, uint64_t>                                              m_resident_handles;
		std::map<std::string, std::shared_ptr<ma_resource_manager_data_source>> m_audio_sources;

		bool m_bindless_supported = false;
	};

} // namespace Boidsish
