#pragma once

#include "procedural_audio.h"
#include <atomic>

namespace Boidsish {

	class AudioManager;

	class RustleAudioEffect : public ProceduralAudioSource {
	public:
		RustleAudioEffect(AudioManager& audioManager);
		virtual ~RustleAudioEffect() = default;

		void OnRead(float* pOutput, ma_uint64 frameCount) override;
		void OnUpdate(float deltaTime, const AudioState& state) override;

	private:
		AudioManager& m_audioManager;

		uint32_t m_xorshiftState;
		float NextWhiteNoise();

		std::atomic<float> m_gain{0.0f};
		std::atomic<float> m_lowPassAlpha{0.1f};

		// Filter state
		float m_low = 0.0f;
	};

} // namespace Boidsish
