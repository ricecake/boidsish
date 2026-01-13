#include "audio_manager.h"

#include "logger.h"
#include "sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include <iostream>
#include <list>
#include <map>
#include <vector>

#include "miniaudio.h"

namespace Boidsish {

	struct AudioManager::AudioManagerImpl {
		ma_engine                       engine;
		bool                                           initialized = false;
		std::list<std::weak_ptr<Sound>>                sounds;
		std::shared_ptr<Sound>                         m_music;
		std::map<std::string, std::shared_ptr<Sound>> m_ambient_sounds;

		AudioManagerImpl() {
			ma_engine_config engineConfig;
			engineConfig = ma_engine_config_init();

			// IMPORTANT: This is enabled for headless testing.
			// Comment out the following line to enable audio playback.
			// engineConfig.noDevice = MA_TRUE; // MA_TRUE means no audio device will be used.
			engineConfig.channels = 2;
			engineConfig.sampleRate = 48000;

			ma_result result = ma_engine_init(&engineConfig, &engine);
			if (result != MA_SUCCESS) {
				logger::ERROR("Failed to initialize audio engine.");
				initialized = false;
			} else {
				initialized = true;
			}
		}

		~AudioManagerImpl() {
			// smart pointers will handle cleanup
			if (initialized) {
				ma_engine_uninit(&engine);
			}
		}
	};

	AudioManager::AudioManager(): m_pimpl(std::make_unique<AudioManagerImpl>()) {}

	AudioManager::~AudioManager() = default;

	void AudioManager::UpdateListener(
		const glm::vec3& position,
		const glm::vec3& direction,
		const glm::vec3& up,
		const float      speed,
		const float      fov
	) {
		if (!m_pimpl->initialized)
			return;

		auto vel = speed * direction;

		ma_engine_listener_set_position(&m_pimpl->engine, 0, position.x, position.y, position.z);
		ma_engine_listener_set_direction(&m_pimpl->engine, 0, direction.x, direction.y, direction.z);
		ma_engine_listener_set_cone(&m_pimpl->engine, 0, glm::radians(fov), glm::radians(fov) * 2, 0.75f);
		ma_engine_listener_set_velocity(&m_pimpl->engine, 0, vel.x, vel.y, vel.z);
		ma_engine_listener_set_world_up(&m_pimpl->engine, 0, up.x, up.y, up.z);
	}

	void AudioManager::PlayMusic(const std::string& filepath, bool loop) {
		if (!m_pimpl->initialized)
			return;
		m_pimpl->m_music = std::make_shared<Sound>(&m_pimpl->engine, filepath, loop, 1.0f, false);
	}

	void AudioManager::PlayAmbientSound(
		const std::string& name, const std::string& filepath, bool loop, float volume
	) {
		if (!m_pimpl->initialized)
			return;
		m_pimpl->m_ambient_sounds[name] =
			std::make_shared<Sound>(&m_pimpl->engine, filepath, loop, volume, false);
	}

	void AudioManager::StopAmbientSound(const std::string& name) {
		if (!m_pimpl->initialized)
			return;
		m_pimpl->m_ambient_sounds.erase(name);
	}

	void AudioManager::SetAmbientSoundVolume(const std::string& name, float volume) {
		if (!m_pimpl->initialized)
			return;
		if (m_pimpl->m_ambient_sounds.count(name)) {
			m_pimpl->m_ambient_sounds[name]->SetVolume(volume);
		}
	}

	std::shared_ptr<Sound>
	AudioManager::CreateSound(const std::string& filepath, const glm::vec3& position, float volume, bool loop) {
		if (!m_pimpl->initialized) {
			return nullptr;
		}

		auto sound = std::make_shared<Sound>(&m_pimpl->engine, filepath, loop, volume, true);
		sound->SetPosition(position);
		m_pimpl->sounds.push_back(sound);
		return sound;
	}

	void AudioManager::Update() {
		if (!m_pimpl->initialized)
			return;

		m_pimpl->sounds.remove_if([](const std::weak_ptr<Sound>& weak_sound) {
			if (auto sound = weak_sound.lock()) {
				return sound->IsDone();
			}
			return true; // Remove if the object is expired
		});
	}

} // namespace Boidsish
