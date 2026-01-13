#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

// Forward declarations for miniaudio types
struct ma_engine;

namespace Boidsish {

	class Sound; // Forward declaration

	class AudioManager {
	public:
		AudioManager();
		~AudioManager();

		// Non-copyable
		AudioManager(const AudioManager&) = delete;
		AudioManager& operator=(const AudioManager&) = delete;

		void UpdateListener(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up);

		// Play a sound that is not spatialized, good for background music
		void PlayMusic(const std::string& filepath, bool loop = true);

		// Create a 3D spatialized sound that can be updated later
		std::shared_ptr<Sound>
		CreateSound(const std::string& filepath, const glm::vec3& position, float volume = 1.0f, bool loop = false);

		void Update();

	private:
		struct AudioManagerImpl;
		std::unique_ptr<AudioManagerImpl> m_pimpl;
	};

} // namespace Boidsish
