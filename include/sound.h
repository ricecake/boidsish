#pragma once

#include <string>
#include <glm/glm.hpp>
#include "miniaudio.h"

namespace Boidsish {

class Sound {
public:
    Sound(ma_engine* engine, const std::string& filepath, bool loop = false, float volume = 1.0f, bool spatialized = true);
    ~Sound();

    // Rule of five: non-copyable, non-movable to keep ownership simple
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    Sound(Sound&&) = delete;
    Sound& operator=(Sound&&) = delete;

    void SetPosition(const glm::vec3& position);
    void SetVolume(float volume);
    void SetLooping(bool loop);
    bool IsDone();

private:
    ma_sound _sound;
    bool _initialized = false;
};

} // namespace Boidsish
