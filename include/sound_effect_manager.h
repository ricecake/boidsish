#pragma once

#include "sound_effect.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Boidsish {

class AudioManager; // Forward declaration

class SoundEffectManager {
public:
    SoundEffectManager(AudioManager* audio_manager);
    ~SoundEffectManager();

    std::shared_ptr<SoundEffect> AddEffect(
        const std::string& filepath,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        float volume = 1.0f,
        bool loop = false,
        float lifetime = -1.0f
    );

    void RemoveEffect(const std::shared_ptr<SoundEffect>& effect);

    void Update(float delta_time);

private:
    AudioManager* _audio_manager;
    std::vector<std::shared_ptr<SoundEffect>> _effects;
};

} // namespace Boidsish
