#pragma once

#include "procedural_audio.h"
#include <random>
#include <atomic>

namespace Boidsish {

	class AudioManager;

	class WindAudioEffect : public ProceduralAudioSource {
	public:
		WindAudioEffect(AudioManager& audioManager);
		virtual ~WindAudioEffect() = default;

		void OnRead(float* pOutput, ma_uint64 frameCount) override;
		void OnUpdate(float deltaTime, const AudioState& state) override;

	private:
		AudioManager& m_audioManager;

		// Audio thread RNG (fast Xorshift)
		uint32_t m_xorshiftState;
		float NextWhiteNoise();

		std::atomic<float> m_gain{0.0f};
		std::atomic<float> m_freq{0.01f};
		std::atomic<float> m_res{0.1f};

		float m_currentWindStrength = 0.0f;

		// State Variable Filter state (audio thread only)
		float m_low = 0.0f;
		float m_band = 0.0f;
	};

} // namespace Boidsish
