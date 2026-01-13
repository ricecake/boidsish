#include "sound_effect.h"

#include "sound.h"

namespace Boidsish {

	SoundEffect::SoundEffect(
		std::shared_ptr<Sound> sound_handle,
		const glm::vec3&       position,
		const glm::vec3&       velocity,
		float                  lifetime
	):
		sound_handle_(sound_handle), position_(position), velocity_(velocity), id_(count++), lifetime_(lifetime) {}

	void SoundEffect::SetPosition(const glm::vec3& pos) {
		position_ = pos;
		if (sound_handle_) {
			sound_handle_->SetPosition(pos);
		}
	}

} // namespace Boidsish
