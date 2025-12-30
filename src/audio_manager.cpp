#include "audio_manager.h"
#include "logger.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <vector>
#include <list>

namespace Boidsish {

struct AudioManager::AudioManagerImpl {
    ma_engine engine;
    bool      initialized = false;
    std::list<ma_sound> sounds;

    AudioManagerImpl() {
        ma_engine_config engineConfig;
        engineConfig = ma_engine_config_init();

        // IMPORTANT: This is enabled for headless testing.
        // Comment out the following line to enable audio playback.
        engineConfig.noDevice = MA_TRUE;
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
        for (auto& sound : sounds) {
            ma_sound_uninit(&sound);
        }
        if (initialized) {
            ma_engine_uninit(&engine);
        }
    }
};

AudioManager::AudioManager() : m_pimpl(std::make_unique<AudioManagerImpl>()) {}

AudioManager::~AudioManager() = default;

void AudioManager::UpdateListener(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up) {
    if (!m_pimpl->initialized) return;

    ma_engine_listener_set_position(&m_pimpl->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_pimpl->engine, 0, direction.x, direction.y, direction.z);
    ma_engine_listener_set_world_up(&m_pimpl->engine, 0, up.x, up.y, up.z);
}

void AudioManager::PlayMusic(const std::string& filepath, bool loop) {
    if (!m_pimpl->initialized) return;

    m_pimpl->sounds.emplace_back();
    ma_sound& sound = m_pimpl->sounds.back();

    ma_result result = ma_sound_init_from_file(&m_pimpl->engine, filepath.c_str(), 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        logger::ERROR("Failed to load music file: {}", filepath);
        m_pimpl->sounds.pop_back();
        return;
    }

    ma_sound_set_spatialization_enabled(&sound, MA_FALSE);
    ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(&sound);
}

void AudioManager::PlaySound(const std::string& filepath, const glm::vec3& position, float volume) {
    if (!m_pimpl->initialized) return;

    m_pimpl->sounds.emplace_back();
    ma_sound& sound = m_pimpl->sounds.back();

    ma_result result = ma_sound_init_from_file(&m_pimpl->engine, filepath.c_str(), 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        logger::ERROR("Failed to load sound file: {}", filepath);
        m_pimpl->sounds.pop_back();
        return;
    }

    ma_sound_set_position(&sound, position.x, position.y, position.z);
    ma_sound_set_volume(&sound, volume);
    ma_sound_set_spatialization_enabled(&sound, MA_TRUE);
    ma_sound_start(&sound);
}

void AudioManager::Update() {
    if (!m_pimpl->initialized) return;

    for (auto it = m_pimpl->sounds.begin(); it != m_pimpl->sounds.end(); ) {
        if (ma_sound_at_end(it)) {
            ma_sound_uninit(it);
            it = m_pimpl->sounds.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Boidsish
