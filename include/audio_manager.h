#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

// Forward declarations for miniaudio types
struct ma_engine;

namespace Boidsish {

	class ServiceLocator;
	class Sound; // Forward declaration
	class ProceduralAudioSource;
	struct AudioState;

	class AudioManager {
	public:
		AudioManager(ServiceLocator& loc);
		~AudioManager();

		// Non-copyable
		AudioManager(const AudioManager&) = delete;
		AudioManager& operator=(const AudioManager&) = delete;

		void UpdateState(const AudioState& state);

		// Play a sound that is not spatialized, good for background music
		void PlayMusic(const std::string& filepath, bool loop = true, float volume = 1.0f);

		void
		PlayAmbientSound(const std::string& name, const std::string& filepath, bool loop = true, float volume = 1.0f);
		void StopAmbientSound(const std::string& name);
		void SetAmbientSoundVolume(const std::string& name, float volume);

		// Create a 3D spatialized sound that can be updated later
		std::shared_ptr<Sound>
		CreateSound(const std::string& filepath, const glm::vec3& position, float volume = 1.0f, bool loop = false);

		// Create a procedural sound
		std::shared_ptr<Sound> CreateProceduralSound(
			std::shared_ptr<ProceduralAudioSource> source,
			const glm::vec3&                       position,
			float                                  volume = 1.0f,
			bool                                   loop = true,
			bool                                   spatialized = true
		);

		void SetGlobalPitch(float pitch);

		float GetMasterVolume() const;
		void  SetMasterVolume(float volume);
		float GetMusicVolume() const;
		void  SetMusicVolume(float volume);
		float GetSfxVolume() const;
		void  SetSfxVolume(float volume);

		AudioState GetCurrentState() const;

		void StopAllSounds();

	private:
		struct AudioManagerImpl;
		std::unique_ptr<AudioManagerImpl> m_pimpl;
	};

} // namespace Boidsish
