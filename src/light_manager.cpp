#include "light_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

#include "biome_properties.h"
#include "profiler.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

	static std::map<char, std::string> morse_alphabet = {
		{'a', ".-"},    {'b', "-..."},   {'c', "-.-."},   {'d', "-.."},    {'e', "."},     {'f', "..-."},
		{'g', "--."},   {'h', "...."},   {'i', ".."},     {'j', ".---"},   {'k', "-.-"},   {'l', ".-.."},
		{'m', "--"},    {'n', "-."},     {'o', "---"},    {'p', ".--."},   {'q', "--.-"},  {'r', ".-."},
		{'s', "..."},   {'t', "-"},      {'u', "..-"},    {'v', "...-"},   {'w', ".--"},   {'x', "-..-"},
		{'y', "-.--"},  {'z', "--.."},   {'1', ".----"},  {'2', "..---"},  {'3', "...--"}, {'4', "....-"},
		{'5', "....."}, {'6', "-...."},  {'7', "--..."},  {'8', "---.."},  {'9', "----."}, {'0', "-----"},
		{' ', "/"},     {',', "--..--"}, {'.', ".-.-.-"}, {'\'', ".----."}
	};

	static void GenerateMorseSequence(Light& light) {
		light.behavior.morse_sequence.clear();
		std::string msg = light.behavior.message;
		std::transform(msg.begin(), msg.end(), msg.begin(), [](unsigned char c) { return std::tolower(c); });

		for (char c : msg) {
			if (morse_alphabet.count(c)) {
				std::string code = morse_alphabet[c];
				if (code == "/") {
					// Word gap: 7 units total. Letter gap is 3, so add 4 more.
					for (int j = 0; j < 4; ++j)
						light.behavior.morse_sequence.push_back(false);
				} else {
					for (char symbol : code) {
						if (symbol == '.') {
							light.behavior.morse_sequence.push_back(true);  // dot
							light.behavior.morse_sequence.push_back(false); // inter-symbol gap
						} else if (symbol == '-') {
							light.behavior.morse_sequence.push_back(true);
							light.behavior.morse_sequence.push_back(true);
							light.behavior.morse_sequence.push_back(true);  // dash
							light.behavior.morse_sequence.push_back(false); // inter-symbol gap
						}
					}
					// Letter gap: 3 units total. Inter-symbol gap is 1, so add 2 more.
					light.behavior.morse_sequence.push_back(false);
					light.behavior.morse_sequence.push_back(false);
				}
			}
		}
	}

	int LightManager::AddLight(const Light& light) {
		Light l = light;
		if (l.id == -1) {
			l.id = _next_light_id++;
		}
		_lights.push_back(l);
		return l.id;
	}

	void LightManager::RemoveLight(int id) {
		if (id == -1)
			return;
		for (auto it = _lights.begin(); it != _lights.end(); ++it) {
			if (it->id == id) {
				_lights.erase(it);
				return;
			}
		}
	}

	Light* LightManager::GetLight(int id) {
		if (id == -1)
			return nullptr;
		for (auto& light : _lights) {
			if (light.id == id) {
				return &light;
			}
		}
		return nullptr;
	}

	std::vector<Light>& LightManager::GetLights() {
		return _lights;
	}

	void LightManager::Update(float deltaTime, ITerrainGenerator* terrain, const glm::vec3& cameraPos) {
		PROJECT_PROFILE_SCOPE("LightManager::Update");

		// Spatially varying ambient probes are now managed per-chunk in TerrainRenderManager

		if (_cycle.enabled) {
			if (!_cycle.paused) {
				_cycle.time += deltaTime * _cycle.speed;
				if (_cycle.time >= 24.0f) {
					_cycle.time -= 24.0f;
					// Randomize moon azimuth for the next cycle (between 45 and 135 degrees)
					_cycle.moon_azimuth = 45.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 90.0f);
				}
				if (_cycle.time < 0.0f) {
					_cycle.time += 24.0f;
					// Randomize if we wrap back (rare but possible)
					_cycle.moon_azimuth = 45.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 90.0f);
				}
			}

			// Update default directional lights (Sun and Moon)
			if (_lights.size() >= 2 && _lights[0].type == DIRECTIONAL_LIGHT && _lights[1].type == DIRECTIONAL_LIGHT) {
				// Sun: Follows standard day/night path
				float sun_angle_deg = (_cycle.time / 24.0f) * 360.0f;
				// t=0 (mid) -> angle=0, elevation=-90
				// t=6 (rise) -> angle=90, elevation=0 (East)
				// t=12 (noon) -> angle=180, elevation=90
				// t=18 (set) -> angle=270, elevation=180 (West)
				_lights[0].elevation = sun_angle_deg - 90.0f;
				_lights[0].azimuth = 90.0f;
				_lights[0].UpdateDirectionFromAngles();

				// Moon: Follows offset path with phase drift for monthly cycle
				// Accumulate fractional days for phase tracking
				if (!_cycle.paused) {
					_cycle.moon_phase_days += deltaTime * _cycle.speed / 24.0f;
				}

				// Phase drift: the moon's offset shifts by 24h over one lunar month
				// This creates the full → half → new → half → full cycle
				float phase_drift = std::fmod(_cycle.moon_phase_days, DayNightCycle::kLunarMonth) /
					DayNightCycle::kLunarMonth * 24.0f;
				float effective_offset = _cycle.moon_offset + phase_drift;

				float moon_time = _cycle.time + effective_offset;
				if (moon_time >= 24.0f)
					moon_time -= 24.0f;
				if (moon_time < 0.0f)
					moon_time += 24.0f;
				float moon_angle_deg = (moon_time / 24.0f) * 360.0f;
				_lights[1].elevation = moon_angle_deg - 90.0f;

				// Moon azimuth drifts slowly (~3°/day) to vary its sky track
				_lights[1].azimuth = _cycle.moon_azimuth + _cycle.moon_phase_days * 3.0f;
				_lights[1].UpdateDirectionFromAngles();

				float sun_vis = glm::sin(glm::radians(_lights[0].elevation));
				float moon_vis = glm::sin(glm::radians(_lights[1].elevation));

				// With physical atmosphere, we don't manually dim the sun.
				// The atmosphere LUTs (transmittance) will handle the color and intensity
				// change naturally. We keep the sun "on" as long as it's not deep below horizon.
				if (sun_vis > -0.1f) {
					_lights[0].base_intensity = 10.0f;
				} else {
					_lights[0].base_intensity = 0.0f;
				}

				// Moon reflects sunlight: brightness and color derive from the sun
				// The sun always illuminates the moon from space regardless of our horizon
				glm::vec3 sunDir = glm::normalize(-_lights[0].direction);
				glm::vec3 moonDir = glm::normalize(-_lights[1].direction);

				// Phase: how much of the illuminated face we see
				// dot = -1 (opposite = full moon), dot = +1 (same side = new moon)
				float cos_phase = glm::dot(sunDir, moonDir);
				float phase = glm::clamp((-cos_phase + 1.0f) * 0.5f, 0.05f, 1.0f);

				// Lunar albedo with slight warm tint from regolith
				const float     lunarAlbedo = _cycle.lunar_albedo;
				const glm::vec3 lunarTint = glm::vec3(0.95f, 0.93f, 0.88f);

				// Moon color = sun's full output × albedo × phase × tint
				glm::vec3 sunFullRadiance = _lights[0].color * 10.0f;
				_lights[1].color = sunFullRadiance * lunarAlbedo * phase * lunarTint;

				if (moon_vis > 0.0f) {
					_lights[1].base_intensity = 1.0f;
				} else {
					_lights[1].base_intensity = 0.0f;
				}

				_cycle.night_factor = glm::smoothstep(0.2f, -0.2f, sun_vis);

				// Fallback ambient — mostly handled by Atmosphere system now
				glm::vec3 day_ambient = Constants::General::Colors::DefaultAmbient();
				glm::vec3 night_ambient = day_ambient * 0.15f;

				// Moon ambient contribution scales with phase and visibility
				night_ambient += _lights[1].color * 0.3f * std::max(0.0f, moon_vis);

				float ambient_factor = std::clamp(sun_vis * 5.0f + 0.5f, 0.0f, 1.0f);
				_ambient_light = glm::mix(night_ambient, day_ambient, ambient_factor);
			}
		}

		for (auto it = _lights.begin(); it != _lights.end();) {
			auto& light = *it;
			if (light.behavior.type == LightBehaviorType::NONE) {
				// We still want to update intensity if it was changed by day/night cycle
				light.intensity = light.base_intensity;
				++it;
				continue;
			}

			light.behavior.timer += deltaTime;

			bool finished = false;
			if (!light.behavior.loop && light.behavior.timer >= light.behavior.period) {
				finished = true;
			}

			switch (light.behavior.type) {
			case LightBehaviorType::BLINK: {
				float phase = fmod(light.behavior.timer, light.behavior.period);
				light.intensity = (phase < light.behavior.period * light.behavior.duty_cycle) ? light.base_intensity
																							  : 0.0f;
				break;
			}
			case LightBehaviorType::PULSE: {
				float sine = sin(2.0f * 3.14159265f * light.behavior.timer / light.behavior.period);
				// Oscillate between 0 and base_intensity if amplitude is 1.0
				light.intensity = light.base_intensity * (1.0f + light.behavior.amplitude * sine) * 0.5f;
				break;
			}
			case LightBehaviorType::EASE_IN: {
				float t = std::min(light.behavior.timer / light.behavior.period, 1.0f);
				float easing = t * t * (3.0f - 2.0f * t); // smoothstep
				light.intensity = light.base_intensity * easing;
				break;
			}
			case LightBehaviorType::EASE_OUT: {
				float t = std::min(light.behavior.timer / light.behavior.period, 1.0f);
				float easing = 1.0f - (t * t * (3.0f - 2.0f * t));
				light.intensity = light.base_intensity * easing;
				break;
			}
			case LightBehaviorType::EASE_IN_OUT: {
				// Turn on and off with easing
				float t = fmod(light.behavior.timer, light.behavior.period) / light.behavior.period;
				float easing = 0.5f - 0.5f * cos(2.0f * 3.14159265f * t);
				light.intensity = light.base_intensity * easing;
				break;
			}
			case LightBehaviorType::FLICKER: {
				float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
				if (light.behavior.flicker_intensity <= 0.5f) {
					// Occasional dimming
					if (r < 0.05f)
						light.intensity = light.base_intensity * (0.2f + 0.3f * (rand() % 100) / 100.0f);
					else
						light.intensity = light.base_intensity;
				} else if (light.behavior.flicker_intensity <= 1.5f) {
					// Mostly off, occasional brightening
					if (r < 0.05f)
						light.intensity = light.base_intensity * (0.5f + 0.5f * (rand() % 100) / 100.0f);
					else
						light.intensity = 0.0f;
				} else {
					// Broken light style
					float chance = std::min(light.behavior.flicker_intensity / 5.0f, 1.0f);
					if (r < chance) {
						light.intensity = light.base_intensity *
							(static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
					}
				}
				break;
			}
			case LightBehaviorType::MORSE: {
				if (light.behavior.morse_index == -1) {
					GenerateMorseSequence(light);
					light.behavior.morse_index = 0;
					light.behavior.timer = 0.0f;
				}
				if (light.behavior.morse_sequence.empty())
					break;

				int index = static_cast<int>(light.behavior.timer / light.behavior.period);
				if (light.behavior.loop) {
					index = index % light.behavior.morse_sequence.size();
				} else if (index >= static_cast<int>(light.behavior.morse_sequence.size())) {
					light.intensity = 0.0f;
					break;
				}
				light.intensity = light.behavior.morse_sequence[index] ? light.base_intensity : 0.0f;
				break;
			}
			default:
				break;
			}

			if (light.auto_remove && finished) {
				it = _lights.erase(it);
			} else {
				++it;
			}
		}
	}

	glm::vec3 LightManager::GetAmbientLight() const {
		return _ambient_light;
	}

	void LightManager::SetAmbientLight(const glm::vec3& ambient) {
		_ambient_light = ambient;
	}

	std::vector<Light*> LightManager::GetShadowCastingLights() {
		std::vector<Light*> shadow_lights;
		for (auto& light : _lights) {
			if (light.casts_shadow && light.intensity > 0.0f) {
				shadow_lights.push_back(&light);
			}
		}
		return shadow_lights;
	}

	int LightManager::GetShadowCastingLightCount() const {
		int count = 0;
		for (const auto& light : _lights) {
			if (light.casts_shadow && light.intensity > 0.0f) {
				++count;
			}
		}
		return count;
	}

} // namespace Boidsish
