#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Boidsish {

class Sound; // Forward declaration

class SoundEffect {
public:
    SoundEffect(
        std::shared_ptr<Sound> sound_handle,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        float lifetime = -1.0f
    );

    void SetPosition(const glm::vec3& pos);
    void SetVelocity(const glm::vec3& vel) { velocity_ = vel; }
    void SetActive(bool active) { active_ = active; }

    const glm::vec3& GetPosition() const { return position_; }
    const glm::vec3& GetVelocity() const { return velocity_; }
    int GetId() const { return id_; }
    bool IsActive() const { return active_; }
    float GetLifetime() const { return lifetime_; }
    void SetLifetime(float lifetime) { lifetime_ = lifetime; }
    float GetLived() const { return lived_; }
    void SetLived(float lived) { lived_ = lived; }

    std::shared_ptr<Sound> GetSoundHandle() { return sound_handle_; }

private:
    inline static int count = 1;
    std::shared_ptr<Sound> sound_handle_;
    glm::vec3         position_;
    glm::vec3         velocity_;
    int               id_;
    bool              active_{true};
    float             lifetime_ = -1.0f;
    float             lived_ = 0.0f;
};

} // namespace Boidsish
