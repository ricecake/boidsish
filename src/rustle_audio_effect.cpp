#include "rustle_audio_effect.h"
#include "audio_manager.h"
#include <algorithm>
#include <cmath>

namespace Boidsish {

	RustleAudioEffect::RustleAudioEffect(AudioManager& audioManager)
		: ProceduralAudioSource(2, 48000),
		  m_audioManager(audioManager),
		  m_xorshiftState(135792468) {
	}

	float RustleAudioEffect::NextWhiteNoise() {
		m_xorshiftState ^= m_xorshiftState << 13;
		m_xorshiftState ^= m_xorshiftState >> 17;
		m_xorshiftState ^= m_xorshiftState << 5;
		return (static_cast<float>(m_xorshiftState) / static_cast<float>(0xFFFFFFFF)) * 2.0f - 1.0f;
	}

	void RustleAudioEffect::OnRead(float* pOutput, ma_uint64 frameCount) {
		float gain = m_gain.load(std::memory_order_relaxed);
		float alpha = m_lowPassAlpha.load(std::memory_order_relaxed);

		if (gain <= 0.0001f) {
			for (ma_uint64 i = 0; i < frameCount * m_channels; ++i) {
				pOutput[i] = 0.0f;
			}
			return;
		}

		for (ma_uint64 i = 0; i < frameCount; ++i) {
			float white = NextWhiteNoise();

			// Simple high-pass filter by subtracting low-pass from signal
			m_low = m_low + alpha * (white - m_low);
			float highPass = white - m_low;

			float sample = highPass * gain;

			for (ma_uint32 c = 0; c < m_channels; ++c) {
				pOutput[i * m_channels + c] = sample;
			}
		}
	}

	void RustleAudioEffect::OnUpdate(float deltaTime, const AudioState& state) {
		// Rustle is proportional to wind strength and grass density
		float strength = std::clamp(state.wind_strength * 0.5f, 0.0f, 1.0f);
		float targetGain = strength * state.grass_density * 0.15f;

		// Interpolate gain
		float currentGain = m_gain.load(std::memory_order_relaxed);
		float nextGain = currentGain + (targetGain - currentGain) * deltaTime * 2.0f;
		m_gain.store(nextGain, std::memory_order_relaxed);

		// Adjust filter alpha with wind strength (higher wind = more broad-spectrum rustle)
		float targetAlpha = 0.02f + strength * 0.1f;
		float currentAlpha = m_lowPassAlpha.load(std::memory_order_relaxed);
		float nextAlpha = currentAlpha + (targetAlpha - currentAlpha) * deltaTime * 1.0f;
		m_lowPassAlpha.store(nextAlpha, std::memory_order_relaxed);
	}

} // namespace Boidsish
