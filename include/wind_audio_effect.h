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
		void OnUpdate(float deltaTime) override;

	private:
		AudioManager& m_audioManager;

		// Separate RNGs for different threads
		std::mt19937 m_updateRng;
		std::mt19937 m_readRng;
		std::uniform_real_distribution<float> m_dist;

		std::atomic<float> m_windStrength{0.0f};
		float m_currentWindStrength = 0.0f;
		float m_targetWindStrength = 0.0f;

		// Simple resonant low-pass filter state (audio thread only)
		float m_v0 = 0.0f;
		float m_v1 = 0.0f;

		std::atomic<float> m_cutoff{0.01f};
		std::atomic<float> m_resonance{0.1f};
	};

} // namespace Boidsish
