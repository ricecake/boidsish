#pragma once

#include "procedural_audio.h"
#include <atomic>

namespace Boidsish {

	class AudioManager;

	class RainAudioEffect : public ProceduralAudioSource {
	public:
		RainAudioEffect(AudioManager& audioManager);
		virtual ~RainAudioEffect() = default;

		void OnRead(float* pOutput, ma_uint64 frameCount) override;
		void OnUpdate(float deltaTime, const AudioState& state) override;

	private:
		AudioManager& m_audioManager;

		uint32_t m_xorshiftState;
		float NextWhiteNoise();

		std::atomic<float> m_intensity{0.0f};

		// Filter state
		float m_lowWash = 0.0f;
		float m_lowPatter = 0.0f;
	};

} // namespace Boidsish
