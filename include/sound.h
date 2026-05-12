#pragma once

#include <string>
#include <memory>

#include "miniaudio.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class ProceduralAudioSource;

	class Sound {
	public:
		Sound(
			ma_engine*         engine,
			const std::string& filepath,
			bool               loop = false,
			float              volume = 1.0f,
			bool               spatialized = true,
			const glm::vec3&   position = {0, 0, 0},
			ma_sound_group*    group = nullptr
		);
		Sound(
			ma_engine*                            engine,
			std::shared_ptr<ProceduralAudioSource> source,
			bool                                  loop = false,
			float                                 volume = 1.0f,
			bool                                  spatialized = true,
			const glm::vec3&                      position = {0, 0, 0},
			ma_sound_group*                       group = nullptr
		);
		~Sound();

		// Rule of five: non-copyable, non-movable to keep ownership simple
		Sound(const Sound&) = delete;
		Sound& operator=(const Sound&) = delete;
		Sound(Sound&&) = delete;
		Sound& operator=(Sound&&) = delete;

		void SetPosition(const glm::vec3& position);
		void SetVolume(float volume);
		void SetPitch(float pitch);
		void SetLooping(bool loop);
		bool IsDone();

		void Stop();

	private:
		ma_sound _sound;
		bool     _initialized = false;
		std::shared_ptr<ProceduralAudioSource> _procedural_source;
	};

} // namespace Boidsish
