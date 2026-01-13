#include "audio_manager.h"

#include "logger.h"
#include "sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include <iostream>
#include <list>
#include <vector>

#include "miniaudio.h"

namespace Boidsish {

	struct AudioManager::AudioManagerImpl {
		ma_engine                  engine;
		bool                       initialized = false;
		std::list<std::shared_ptr<Sound>> sounds;

		AudioManagerImpl() {
			ma_engine_config engineConfig;
			engineConfig = ma_engine_config_init();

			// IMPORTANT: This is enabled for headless testing.
			// Comment out the following line to enable audio playback.
			// engineConfig.noDevice = MA_TRUE;
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

	void AudioManager::UpdateListener(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up) {
		if (!m_pimpl->initialized)
			return;

		ma_engine_listener_set_position(&m_pimpl->engine, 0, position.x, position.y, position.z);
		ma_engine_listener_set_direction(&m_pimpl->engine, 0, direction.x, direction.y, direction.z);
		ma_engine_listener_set_world_up(&m_pimpl->engine, 0, up.x, up.y, up.z);
	}

	void AudioManager::PlayMusic(const std::string& filepath, bool loop) {
		if (!m_pimpl->initialized)
			return;

        auto sound = std::make_shared<Sound>(&m_pimpl->engine, filepath, loop, 1.0f, false);
		m_pimpl->sounds.push_back(sound);
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

		m_pimpl->sounds.remove_if([](const std::shared_ptr<Sound>& sound) {
			return sound->IsDone();
		});
	}

    ma_engine* AudioManager::GetEngine() {
        if (!m_pimpl->initialized) return nullptr;
        return &m_pimpl->engine;
    }

} // namespace Boidsish
