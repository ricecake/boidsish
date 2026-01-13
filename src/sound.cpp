#include "sound.h"
#include "logger.h"

namespace Boidsish {

Sound::Sound(ma_engine* engine, const std::string& filepath, bool loop, float volume, bool spatialized) {
    if (!engine) {
        logger::ERROR("Sound created with null audio engine.");
        return;
    }

    ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), 0, NULL, NULL, &_sound);
    if (result != MA_SUCCESS) {
        logger::ERROR("Failed to load sound file: {}", filepath);
        return;
    }

    SetVolume(volume);
    SetLooping(loop);
    ma_sound_set_spatialization_enabled(&_sound, spatialized ? MA_TRUE : MA_FALSE);

    ma_sound_start(&_sound);
    _initialized = true;
}

Sound::~Sound() {
    if (_initialized) {
        ma_sound_uninit(&_sound);
    }
}

void Sound::SetPosition(const glm::vec3& position) {
    if (_initialized) {
        ma_sound_set_position(&_sound, position.x, position.y, position.z);
    }
}

void Sound::SetVolume(float volume) {
    if (_initialized) {
        ma_sound_set_volume(&_sound, volume);
    }
}

void Sound::SetLooping(bool loop) {
    if (_initialized) {
        ma_sound_set_looping(&_sound, loop ? MA_TRUE : MA_FALSE);
    }
}

bool Sound::IsDone() {
    if (!_initialized) {
        return true;
    }
    return ma_sound_at_end(&_sound) ? true : false;
}

} // namespace Boidsish
