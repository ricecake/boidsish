#include "wind_audio_effect.h"
#include "audio_manager.h"
#include <algorithm>
#include <cmath>

namespace Boidsish {

	WindAudioEffect::WindAudioEffect(AudioManager& audioManager)
		: ProceduralAudioSource(2, 48000),
		  m_audioManager(audioManager),
		  m_updateRng(std::random_device{}()),
		  m_readRng(std::random_device{}()),
		  m_dist(-1.0f, 1.0f) {
	}

	void WindAudioEffect::OnRead(float* pOutput, ma_uint64 frameCount) {
		float f = m_cutoff.load(std::memory_order_relaxed);
		float q = m_resonance.load(std::memory_order_relaxed);
		float strength = m_windStrength.load(std::memory_order_relaxed);

		for (ma_uint64 i = 0; i < frameCount; ++i) {
			float whiteNoise = m_dist(m_readRng);

			// Resonant Low-Pass Filter
			m_v0 = (1.0f - f * q) * m_v0 - f * m_v1 + f * whiteNoise;
			m_v1 = (1.0f - f * q) * m_v1 + f * m_v0;

			float sample = m_v1 * strength * 0.5f;

			// Output to all channels
			for (ma_uint32 c = 0; c < m_channels; ++c) {
				pOutput[i * m_channels + c] = sample;
			}
		}
	}

	void WindAudioEffect::OnUpdate(float deltaTime) {
		AudioState state = m_audioManager.GetCurrentState();

		m_targetWindStrength = std::clamp(state.wind_strength * 0.2f, 0.0f, 1.0f);

		// Smoothly interpolate wind strength
		m_currentWindStrength += (m_targetWindStrength - m_currentWindStrength) * deltaTime * 2.0f;
		m_windStrength.store(m_currentWindStrength, std::memory_order_relaxed);

		// Modulate filter parameters
		// Wind strength increases the "howl" frequency and resonance
		float cutoff = 0.005f + m_currentWindStrength * 0.05f + std::abs(m_dist(m_updateRng)) * 0.01f;
		float resonance = 0.01f + m_currentWindStrength * 0.15f;

		m_cutoff.store(cutoff, std::memory_order_relaxed);
		m_resonance.store(resonance, std::memory_order_relaxed);
	}

} // namespace Boidsish
