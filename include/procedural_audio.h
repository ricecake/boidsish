#pragma once

#include "miniaudio.h"

namespace Boidsish {

	class ProceduralAudioSource {
	public:
		ProceduralAudioSource(ma_uint32 channels = 2, ma_uint32 sampleRate = 48000);
		virtual ~ProceduralAudioSource();

		// Non-copyable
		ProceduralAudioSource(const ProceduralAudioSource&) = delete;
		ProceduralAudioSource& operator=(const ProceduralAudioSource&) = delete;

		virtual void OnRead(float* pOutput, ma_uint64 frameCount) = 0;
		virtual void OnUpdate(float deltaTime) {}

		ma_data_source* GetDataSource() { return &m_base; }

		ma_uint32 GetChannels() const { return m_channels; }
		ma_uint32 GetSampleRate() const { return m_sampleRate; }

	protected:
		ma_data_source_base m_base;
		ma_uint32           m_channels;
		ma_uint32           m_sampleRate;
	};

} // namespace Boidsish
