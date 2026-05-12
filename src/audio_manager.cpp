#include "audio_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

#include "logger.h"
#include "procedural_audio.h"
#include "profiler.h"
#include "service_locator.h"
#include "sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef ERROR
	#undef ERROR
#endif

namespace Boidsish {

	struct AudioManager::AudioManagerImpl {
		ma_engine                                     engine;
		ma_sound_group                                master_group;
		ma_sound_group                                music_group;
		ma_sound_group                                sfx_group;

		bool                                          initialized = false;
		bool                                          groups_initialized = false;

		std::list<std::weak_ptr<Sound>>               sounds;
		std::shared_ptr<Sound>                        m_music;
		std::map<std::string, std::shared_ptr<Sound>> m_ambient_sounds;
		std::list<std::weak_ptr<ProceduralAudioSource>> m_procedural_sources;

		std::mutex                                    m_sounds_mutex;
		std::mutex                                    m_state_mutex;
		AudioState                                    m_current_state;

		std::thread                                   m_audio_thread;
		std::atomic<bool>                             m_running{false};

		AudioManagerImpl() {
			ma_engine_config engineConfig = ma_engine_config_init();
			engineConfig.channels = 2;
			engineConfig.sampleRate = 48000;

			ma_result result = ma_engine_init(&engineConfig, &engine);
			if (result != MA_SUCCESS) {
				logger::ERROR("Failed to initialize audio engine.");
				initialized = false;
				return;
			}
			initialized = true;

			ma_sound_group_init(&engine, 0, NULL, &master_group);
			ma_sound_group_init(&engine, 0, &master_group, &music_group);
			ma_sound_group_init(&engine, 0, &master_group, &sfx_group);
			groups_initialized = true;

			m_running = true;
			m_audio_thread = std::thread(&AudioManagerImpl::AudioLoop, this);
		}

		~AudioManagerImpl() {
			m_running = false;
			if (m_audio_thread.joinable()) {
				m_audio_thread.join();
			}

			m_music.reset();
			m_ambient_sounds.clear();
			sounds.clear();
			m_procedural_sources.clear();

			if (groups_initialized) {
				ma_sound_group_uninit(&sfx_group);
				ma_sound_group_uninit(&music_group);
				ma_sound_group_uninit(&master_group);
			}

			if (initialized) {
				ma_engine_uninit(&engine);
			}
		}

		void AudioLoop() {
			auto last_time = std::chrono::high_resolution_clock::now();

			while (m_running) {
				auto current_time = std::chrono::high_resolution_clock::now();
				float delta_time = std::chrono::duration<float>(current_time - last_time).count();
				last_time = current_time;

				Update(delta_time);

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		void Update(float delta_time) {
			if (!initialized) return;

			AudioState state;
			{
				std::lock_guard<std::mutex> lock(m_state_mutex);
				state = m_current_state;
			}

			// Update Listener
			auto vel = state.listener_speed * state.listener_front;
			ma_engine_listener_set_position(&engine, 0, state.listener_pos.x, state.listener_pos.y, state.listener_pos.z);
			ma_engine_listener_set_direction(&engine, 0, state.listener_front.x, state.listener_front.y, state.listener_front.z);
			ma_engine_listener_set_world_up(&engine, 0, state.listener_up.x, state.listener_up.y, state.listener_up.z);
			ma_engine_listener_set_velocity(&engine, 0, vel.x, vel.y, vel.z);
			ma_engine_listener_set_cone(&engine, 0, glm::radians(state.listener_fov), glm::radians(state.listener_fov) * 2, 0.75f);

			// Update Volumes/Pitch
			if (groups_initialized) {
				ma_sound_group_set_volume(&master_group, state.master_volume);
				ma_sound_group_set_volume(&music_group, state.music_volume);
				ma_sound_group_set_volume(&sfx_group, state.sfx_volume);
				ma_sound_group_set_pitch(&master_group, state.global_pitch);
			}

			// Update procedural sources
			{
				std::lock_guard<std::mutex> lock(m_sounds_mutex);
				m_procedural_sources.remove_if([delta_time](const std::weak_ptr<ProceduralAudioSource>& weak_source) {
					if (auto source = weak_source.lock()) {
						source->OnUpdate(delta_time);
						return false;
					}
					return true;
				});

				// Cleanup finished sounds
				sounds.remove_if([](const std::weak_ptr<Sound>& weak_sound) {
					if (auto sound = weak_sound.lock()) {
						return sound->IsDone();
					}
					return true;
				});
			}
		}
	};

	AudioManager::AudioManager(ServiceLocator& /*loc*/): m_pimpl(std::make_unique<AudioManagerImpl>()) {}

	AudioManager::~AudioManager() = default;

	void AudioManager::UpdateState(const AudioState& state) {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		m_pimpl->m_current_state = state;
	}

	void AudioManager::PlayMusic(const std::string& filepath, bool loop, float volume) {
		if (!m_pimpl->initialized) return;
		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		m_pimpl->m_music = std::make_shared<Sound>(
			&m_pimpl->engine,
			filepath,
			loop,
			volume,
			false,
			glm::vec3(0.0f),
			m_pimpl->groups_initialized ? &m_pimpl->music_group : nullptr
		);
	}

	void AudioManager::PlayAmbientSound(const std::string& name, const std::string& filepath, bool loop, float volume) {
		if (!m_pimpl->initialized) return;
		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		m_pimpl->m_ambient_sounds[name] = std::make_shared<Sound>(
			&m_pimpl->engine,
			filepath,
			loop,
			volume,
			false,
			glm::vec3(0.0f),
			m_pimpl->groups_initialized ? &m_pimpl->sfx_group : nullptr
		);
	}

	void AudioManager::StopAmbientSound(const std::string& name) {
		if (!m_pimpl->initialized) return;
		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		m_pimpl->m_ambient_sounds.erase(name);
	}

	void AudioManager::SetAmbientSoundVolume(const std::string& name, float volume) {
		if (!m_pimpl->initialized) return;
		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		if (m_pimpl->m_ambient_sounds.count(name)) {
			m_pimpl->m_ambient_sounds[name]->SetVolume(volume);
		}
	}

	std::shared_ptr<Sound>
	AudioManager::CreateSound(const std::string& filepath, const glm::vec3& position, float volume, bool loop) {
		if (!m_pimpl->initialized) return nullptr;

		auto sound = std::make_shared<Sound>(
			&m_pimpl->engine,
			filepath,
			loop,
			volume,
			true,
			position,
			m_pimpl->groups_initialized ? &m_pimpl->sfx_group : nullptr
		);

		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		m_pimpl->sounds.push_back(sound);
		return sound;
	}

	std::shared_ptr<Sound> AudioManager::CreateProceduralSound(
		std::shared_ptr<ProceduralAudioSource> source,
		const glm::vec3&                       position,
		float                                  volume,
		bool                                   loop,
		bool                                   spatialized
	) {
		if (!m_pimpl->initialized || !source) return nullptr;

		auto sound = std::make_shared<Sound>(
			&m_pimpl->engine,
			source,
			loop,
			volume,
			spatialized,
			position,
			m_pimpl->groups_initialized ? &m_pimpl->sfx_group : nullptr
		);

		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);
		m_pimpl->m_procedural_sources.push_back(source);
		m_pimpl->sounds.push_back(sound);
		return sound;
	}

	void AudioManager::SetGlobalPitch(float pitch) {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		m_pimpl->m_current_state.global_pitch = pitch;
	}

	float AudioManager::GetMasterVolume() const {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		return m_pimpl->m_current_state.master_volume;
	}

	void AudioManager::SetMasterVolume(float volume) {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		m_pimpl->m_current_state.master_volume = volume;
	}

	float AudioManager::GetMusicVolume() const {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		return m_pimpl->m_current_state.music_volume;
	}

	void AudioManager::SetMusicVolume(float volume) {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		m_pimpl->m_current_state.music_volume = volume;
	}

	float AudioManager::GetSfxVolume() const {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		return m_pimpl->m_current_state.sfx_volume;
	}

	void AudioManager::SetSfxVolume(float volume) {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		m_pimpl->m_current_state.sfx_volume = volume;
	}

	AudioState AudioManager::GetCurrentState() const {
		std::lock_guard<std::mutex> lock(m_pimpl->m_state_mutex);
		return m_pimpl->m_current_state;
	}

	void AudioManager::StopAllSounds() {
		if (!m_pimpl->initialized) return;
		std::lock_guard<std::mutex> lock(m_pimpl->m_sounds_mutex);

		if (m_pimpl->m_music) m_pimpl->m_music->Stop();
		for (auto& pair : m_pimpl->m_ambient_sounds) {
			if (pair.second) pair.second->Stop();
		}
		for (auto& weak_sound : m_pimpl->sounds) {
			if (auto sound = weak_sound.lock()) {
				sound->Stop();
			}
		}

		m_pimpl->m_music.reset();
		m_pimpl->m_ambient_sounds.clear();
		m_pimpl->sounds.clear();
		m_pimpl->m_procedural_sources.clear();
	}

} // namespace Boidsish
