#include "light_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

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

	void LightManager::AddLight(const Light& light) {
		_lights.push_back(light);
	}

	std::vector<Light>& LightManager::GetLights() {
		return _lights;
	}

	void LightManager::SetDayNightState(int /*dayCount*/, float factor) {
		// Ensure we have at least two directional lights for Sun and Moon
		// The first one is the Sun, the second one is the Moon.

		if (!_day_night_setup) {
			// Clear existing if any to ensure Sun/Moon are first (risky if user added lights early)
			// Actually, let's just insert them at the front or ensure they exist.
			bool sun_found = false, moon_found = false;
			if (_lights.size() >= 1 && _lights[0].type == DIRECTIONAL_LIGHT) sun_found = true;
			if (_lights.size() >= 2 && _lights[1].type == DIRECTIONAL_LIGHT) moon_found = true;

			if (!sun_found) {
				_lights.insert(_lights.begin(), Light::CreateDirectional({0, 100, 0}, {0, -1, 0}, 1.0f, {1, 0.9f, 0.8f}, true));
			}
			if (!moon_found) {
				_lights.insert(_lights.begin() + 1, Light::CreateDirectional({0, 100, 0}, {0, -1, 0}, 0.2f, {0.5f, 0.6f, 1.0f}, false));
			}
			_day_night_setup = true;
		}

		if (_lights.size() >= 2) {
			float pi = 3.14159265f;
			// Calculate Sun direction
			// factor 0.0 (noon) -> Sun is at top (0, -1, 0)
			// factor 0.5 (dusk/dawn) -> Sun is at horizon
			// We want the sun to rotate around the X axis or similar.

			// Use progress instead of factor for rotation
			// factor = 0.5 - 0.5 * cos(2*PI * (progress - 0.25))
			// We can reconstruct progress approximately or just use factor.
			// Actually, it's better if SetDayNightState took progress too.
			// But I can derive an angle from factor and some extra state.
			// Let's assume factor is enough for intensity, but direction needs more.

			// Wait, the progress was (progress - 0.25) in the cos.
			// Let's just use the factor to adjust intensities and a simple rotation.

			float sun_intensity = std::max(0.0f, 1.0f - factor * 2.0f); // Simple linear fade
			// Actually, let's use a better one:
			sun_intensity = std::clamp(1.5f - factor * 4.0f, 0.0f, 1.0f);

			float moon_intensity = std::clamp((factor - 0.5f) * 4.0f, 0.0f, 0.3f);

			_lights[0].intensity = sun_intensity;
			_lights[1].intensity = moon_intensity;

			// Rotate Sun and Moon
			// Noon (factor 0) -> Sun at (0, -1, 0)
			// Midnight (factor 1) -> Moon at (0, -1, 0)

			float angle = factor * pi;
			_lights[0].direction = glm::normalize(glm::vec3(sin(angle), -cos(angle), 0.0f));
			_lights[1].direction = glm::normalize(glm::vec3(sin(angle + pi), -cos(angle + pi), 0.0f));

			// Adjust ambient light
			glm::vec3 day_ambient(90.0f/255.0f, 81.0f/255.0f, 62.0f/255.0f);
			glm::vec3 night_ambient(10.0f/255.0f, 15.0f/255.0f, 30.0f/255.0f);
			_ambient_light = glm::mix(day_ambient, night_ambient, factor);
		}
	}

	void LightManager::Update(float deltaTime) {
		for (auto& light : _lights) {
			if (light.behavior.type == LightBehaviorType::NONE) {
				continue;
			}

			light.behavior.timer += deltaTime;

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
			if (light.casts_shadow) {
				shadow_lights.push_back(&light);
			}
		}
		return shadow_lights;
	}

	int LightManager::GetShadowCastingLightCount() const {
		int count = 0;
		for (const auto& light : _lights) {
			if (light.casts_shadow) {
				++count;
			}
		}
		return count;
	}

} // namespace Boidsish
