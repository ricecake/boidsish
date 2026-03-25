#include "weather_manager.h"

#include <algorithm>

#include "Simplex.h"
#include "profiler.h"

namespace Boidsish {

	WeatherManager::WeatherManager() {
		InitializePresets();
	}

	WeatherManager::~WeatherManager() {}

	void WeatherManager::InitializePresets() {
		// 1. Sunny
		WeatherPreset sunny;
		sunny.name = "Sunny";
		sunny.weight = 10.0f;
		sunny.settings.sun_intensity = {0.8f, 1.2f};
		sunny.settings.wind_strength = {0.02f, 0.08f};
		sunny.settings.wind_speed = {0.05f, 0.15f};
		sunny.settings.wind_frequency = {0.01f, 0.03f};
		sunny.settings.cloud_density = {0.0f, 0.2f};
		sunny.settings.cloud_altitude = {180.0f, 220.0f};
		sunny.settings.cloud_thickness = {5.0f, 15.0f};
		sunny.settings.haze_density = {0.001f, 0.003f};
		sunny.settings.haze_height = {10.0f, 30.0f};
		sunny.settings.rayleigh_scale = {1.0f, 1.2f};
		sunny.settings.mie_scale = {0.05f, 0.15f};
		presets_.push_back(sunny);

		// 2. Cloudy
		WeatherPreset cloudy;
		cloudy.name = "Cloudy";
		cloudy.weight = 6.0f;
		cloudy.settings.sun_intensity = {0.4f, 0.7f};
		cloudy.settings.wind_strength = {0.1f, 0.2f};
		cloudy.settings.wind_speed = {0.2f, 0.4f};
		cloudy.settings.wind_frequency = {0.02f, 0.05f};
		cloudy.settings.cloud_density = {0.4f, 0.7f};
		cloudy.settings.cloud_altitude = {150.0f, 180.0f};
		cloudy.settings.cloud_thickness = {20.0f, 40.0f};
		cloudy.settings.haze_density = {0.004f, 0.008f};
		cloudy.settings.haze_height = {30.0f, 60.0f};
		cloudy.settings.rayleigh_scale = {1.2f, 1.5f};
		cloudy.settings.mie_scale = {0.2f, 0.4f};
		presets_.push_back(cloudy);

		// 3. Overcast
		WeatherPreset overcast;
		overcast.name = "Overcast";
		overcast.weight = 4.0f;
		overcast.settings.sun_intensity = {0.2f, 0.4f};
		overcast.settings.wind_strength = {0.2f, 0.5f};
		overcast.settings.wind_speed = {0.4f, 0.8f};
		overcast.settings.wind_frequency = {0.04f, 0.08f};
		overcast.settings.cloud_density = {0.8f, 1.0f};
		overcast.settings.cloud_altitude = {100.0f, 140.0f};
		overcast.settings.cloud_thickness = {40.0f, 70.0f};
		overcast.settings.haze_density = {0.01f, 0.02f};
		overcast.settings.haze_height = {50.0f, 100.0f};
		overcast.settings.rayleigh_scale = {1.5f, 2.0f};
		overcast.settings.mie_scale = {0.5f, 1.0f};
		presets_.push_back(overcast);

		// 4. Foggy
		WeatherPreset foggy;
		foggy.name = "Foggy";
		foggy.weight = 2.0f;
		foggy.settings.sun_intensity = {0.1f, 0.3f};
		foggy.settings.wind_strength = {0.01f, 0.05f};
		foggy.settings.wind_speed = {0.01f, 0.1f};
		foggy.settings.wind_frequency = {0.005f, 0.02f};
		foggy.settings.cloud_density = {0.3f, 0.6f};
		foggy.settings.cloud_altitude = {200.0f, 300.0f};
		foggy.settings.cloud_thickness = {10.0f, 30.0f};
		foggy.settings.haze_density = {0.03f, 0.06f};
		foggy.settings.haze_height = {80.0f, 150.0f};
		foggy.settings.rayleigh_scale = {2.0f, 3.0f};
		foggy.settings.mie_scale = {1.5f, 4.0f};
		presets_.push_back(foggy);
	}

	void WeatherManager::Update(float deltaTime, float totalTime, const glm::vec3& cameraPos) {
		if (!enabled_)
			return;

		PROJECT_PROFILE_SCOPE("WeatherManager::Update");

		// Calculate weather control value using low-frequency noise
		glm::vec2 noisePos = glm::vec2(cameraPos.x, cameraPos.z) * spatial_scale_ + glm::vec2(totalTime * time_scale_);

		float controlValue = Simplex::noise(noisePos) * 0.5f + 0.5f;

		// Blend ranges based on control value
		if (presets_.empty())
			return;

		float              totalWeight = 0.0f;
		std::vector<float> cdf;
		for (const auto& p : presets_) {
			totalWeight += p.weight;
			cdf.push_back(totalWeight);
		}

		float target = std::clamp(controlValue, 0.0f, 1.0f) * totalWeight;
		auto  it = std::upper_bound(cdf.begin(), cdf.end(), target);
		int   highIdx = std::distance(cdf.begin(), it);
		int   lowIdx = std::max(0, highIdx - 1);
		highIdx = std::min(highIdx, (int)presets_.size() - 1);

		float weightHigh = cdf[highIdx];
		float weightLow = (highIdx == 0) ? 0.0f : cdf[highIdx - 1];
		float segmentWidth = weightHigh - weightLow;
		float t = (segmentWidth > 0.0001f) ? (target - weightLow) / segmentWidth : 0.0f;

		const auto& lowPreset = presets_[lowIdx].settings;
		const auto& highPreset = presets_[highIdx].settings;

		// Interpolated settings (blended ranges)
		WeatherSettings blended = lowPreset * (1.0f - t) + highPreset * t;

		// Use secondary noise to pick values within the blended ranges
		// We use slightly higher frequency or different offsets for variety
		auto sampleNoise = [&](float offset) {
			return Simplex::noise(noisePos * 2.0f + glm::vec2(offset)) * 0.5f + 0.5f;
		};

		current_.sun_intensity = blended.sun_intensity.Lerp(sampleNoise(1.1f));
		current_.wind_strength = blended.wind_strength.Lerp(sampleNoise(2.2f));
		current_.wind_speed = blended.wind_speed.Lerp(sampleNoise(3.3f));
		current_.wind_frequency = blended.wind_frequency.Lerp(sampleNoise(4.4f));
		current_.cloud_density = blended.cloud_density.Lerp(sampleNoise(5.5f));
		current_.cloud_altitude = blended.cloud_altitude.Lerp(sampleNoise(6.6f));
		current_.cloud_thickness = blended.cloud_thickness.Lerp(sampleNoise(7.7f));
		current_.haze_density = blended.haze_density.Lerp(sampleNoise(8.8f));
		current_.haze_height = blended.haze_height.Lerp(sampleNoise(9.9f));
		current_.rayleigh_scale = blended.rayleigh_scale.Lerp(sampleNoise(10.10f));
		current_.mie_scale = blended.mie_scale.Lerp(sampleNoise(11.11f));
	}

} // namespace Boidsish
