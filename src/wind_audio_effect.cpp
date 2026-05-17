#include "wind_audio_effect.h"
#include "audio_manager.h"
#include <algorithm>
#include <cmath>

namespace Boidsish {

	WindAudioEffect::WindAudioEffect(AudioManager& audioManager)
		: ProceduralAudioSource(2, 48000),
		  m_audioManager(audioManager),
		  m_xorshiftState(123456789) {
	}

	float WindAudioEffect::NextWhiteNoise() {
		m_xorshiftState ^= m_xorshiftState << 13;
		m_xorshiftState ^= m_xorshiftState >> 17;
		m_xorshiftState ^= m_xorshiftState << 5;
		return (static_cast<float>(m_xorshiftState) / static_cast<float>(0xFFFFFFFF)) * 2.0f - 1.0f;
	}

	void WindAudioEffect::OnRead(float* pOutput, ma_uint64 frameCount) {
		// SVF coefficients from https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
		// simplified for real-time wind
		float f = m_freq.load(std::memory_order_relaxed);
		float res = m_res.load(std::memory_order_relaxed);
		float gain = m_gain.load(std::memory_order_relaxed);

		// Pre-calculate dampening from resonance
		float damping = 1.0f / (res + 0.001f);

		for (ma_uint64 i = 0; i < frameCount; ++i) {
			float input = NextWhiteNoise();

			// SVF Step
			m_low = m_low + f * m_band;
			float high = input - m_low - damping * m_band;
			m_band = m_band + f * high;

			// Use bandpass for a more "whistling" wind effect
			float sample = m_band * gain;

			for (ma_uint32 c = 0; c < m_channels; ++c) {
				pOutput[i * m_channels + c] = sample;
			}
		}
	}

	void WindAudioEffect::OnUpdate(float deltaTime, const AudioState& state) {
		float targetStrength = std::clamp(state.wind_strength * 2.0f, 0.0f, 10.0f);

		// Responsive interpolation
		m_currentWindStrength += (targetStrength - m_currentWindStrength) * deltaTime * 3.0f;

		// Map strength to parameters
		// Wind starts being audible even at low strengths
		float gain = std::clamp(m_currentWindStrength, 0.0f, 0.5f);

		// Cutoff frequency moves between 100Hz and 2000Hz (normalized roughly)
		float freq = 0.005f + std::clamp(m_currentWindStrength * 0.02f, 0.0f, 0.1f);

		// Resonance increases with strength for that "howl"
		float resonance = 0.5f + m_currentWindStrength * 0.2f;

		m_gain.store(gain, std::memory_order_relaxed);
		m_freq.store(freq, std::memory_order_relaxed);
		m_res.store(resonance, std::memory_order_relaxed);
	}

} // namespace Boidsish
