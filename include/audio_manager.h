#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>

// Forward declarations for miniaudio types
typedef struct ma_engine ma_engine;
typedef struct ma_sound ma_sound;

namespace Boidsish {

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

    // Play a 3D spatialized sound at a specific position
    void PlaySound(const std::string& filepath, const glm::vec3& position, float volume = 1.0f);

private:
    struct AudioManagerImpl;
    std::unique_ptr<AudioManagerImpl> m_pimpl;
};

} // namespace Boidsish
