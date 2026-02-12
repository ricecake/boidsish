#include "sound_effect_manager.h"

#include <algorithm>

#include "audio_manager.h"
#include "sound.h"

namespace Boidsish {

	SoundEffectManager::SoundEffectManager(AudioManager* audio_manager): _audio_manager(audio_manager) {}

	SoundEffectManager::~SoundEffectManager() {
		// Clear all effects before destruction to ensure Sound objects
		// are released while AudioManager is still valid
		std::lock_guard<std::mutex> lock(_mutex);
		_effects.clear();
	}

	std::shared_ptr<SoundEffect> SoundEffectManager::AddEffect(
		const std::string& filepath,
		const glm::vec3&   position,
		const glm::vec3&   velocity,
		float              volume,
		bool               loop,
		float              lifetime
	) {
		if (!_audio_manager) {
			return nullptr;
		}

		auto sound_handle = _audio_manager->CreateSound(filepath, position, volume, loop);
		if (!sound_handle) {
			return nullptr;
		}

		auto                        effect = std::make_shared<SoundEffect>(sound_handle, position, velocity, lifetime);
		std::lock_guard<std::mutex> lock(_mutex);
		_effects.push_back(effect);
		return effect;
	}

	void SoundEffectManager::RemoveEffect(const std::shared_ptr<SoundEffect>& effect) {
		if (effect) {
			effect->SetActive(false); // Mark as inactive
			                          // The main update loop will remove it.
		}
	}

	void SoundEffectManager::Update(float delta_time) {
		std::lock_guard<std::mutex> lock(_mutex);
		// Update positions and lifetimes
		for (auto& effect : _effects) {
			if (effect && effect->IsActive()) {
				// Update lifetime
				float lifetime = effect->GetLifetime();
				if (lifetime > 0.0f) {
					float lived = effect->GetLived();
					lived += delta_time;
					effect->SetLived(lived);
					if (lived >= lifetime) {
						effect->SetActive(false);
					}
				}

				// Update position
				glm::vec3 new_pos = effect->GetPosition() + effect->GetVelocity() * delta_time;
				effect->SetPosition(new_pos);
			}
		}

		// Remove inactive or finished effects
		_effects.erase(
			std::remove_if(
				_effects.begin(),
				_effects.end(),
				[](const std::shared_ptr<SoundEffect>& effect) {
					return !effect || !effect->IsActive() ||
						(effect->GetSoundHandle() && effect->GetSoundHandle()->IsDone());
				}
			),
			_effects.end()
		);
	}

} // namespace Boidsish
