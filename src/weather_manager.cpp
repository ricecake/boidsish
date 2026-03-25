#include "weather_manager.h"

#include <algorithm>
#include <cmath>

#include "Simplex.h"
#include "profiler.h"

namespace Boidsish {

	WeatherManager::WeatherManager() {
		InitializePresets();

		// Initialize default paces for various attributes
		SetPace(WeatherAttribute::SunIntensity, 0.5f);
		SetPace(WeatherAttribute::WindStrength, 0.13f);
		SetPace(WeatherAttribute::WindSpeed, 0.13f);
		SetPace(WeatherAttribute::WindFrequency, 0.05f);
		SetPace(WeatherAttribute::CloudDensity, 0.2f);
		SetPace(WeatherAttribute::CloudAltitude, 0.1f);
		SetPace(WeatherAttribute::CloudThickness, 0.1f);
		SetPace(WeatherAttribute::HazeDensity, 0.2f);
		SetPace(WeatherAttribute::HazeHeight, 0.1f);
		SetPace(WeatherAttribute::RayleighScale, 0.1f);
		SetPace(WeatherAttribute::MieScale, 0.1f);
	}

	WeatherManager::~WeatherManager() {}

	void WeatherManager::SetTarget(WeatherAttribute attr, float target) {
		if (attr == WeatherAttribute::Count)
			return;
		attribute_states_[static_cast<size_t>(attr)].external_target = target;
	}

	void WeatherManager::ClearTarget(WeatherAttribute attr) {
		if (attr == WeatherAttribute::Count)
			return;
		attribute_states_[static_cast<size_t>(attr)].external_target = std::nullopt;
	}

	void WeatherManager::SetPace(WeatherAttribute attr, float pace) {
		if (attr == WeatherAttribute::Count)
			return;
		attribute_states_[static_cast<size_t>(attr)].omega = pace;
	}

	void WeatherManager::UpdateAttribute(WeatherAttribute attr, float target, float deltaTime) {
		if (attr == WeatherAttribute::Count || deltaTime <= 0.0f)
			return;

		auto&  state = attribute_states_[static_cast<size_t>(attr)];
		float* value_ptr = nullptr;

		switch (attr) {
		case WeatherAttribute::SunIntensity:
			value_ptr = &current_.sun_intensity;
			break;
		case WeatherAttribute::WindStrength:
			value_ptr = &current_.wind_strength;
			break;
		case WeatherAttribute::WindSpeed:
			value_ptr = &current_.wind_speed;
			break;
		case WeatherAttribute::WindFrequency:
			value_ptr = &current_.wind_frequency;
			break;
		case WeatherAttribute::CloudDensity:
			value_ptr = &current_.cloud_density;
			break;
		case WeatherAttribute::CloudAltitude:
			value_ptr = &current_.cloud_altitude;
			break;
		case WeatherAttribute::CloudThickness:
			value_ptr = &current_.cloud_thickness;
			break;
		case WeatherAttribute::HazeDensity:
			value_ptr = &current_.haze_density;
			break;
		case WeatherAttribute::HazeHeight:
			value_ptr = &current_.haze_height;
			break;
		case WeatherAttribute::RayleighScale:
			value_ptr = &current_.rayleigh_scale;
			break;
		case WeatherAttribute::MieScale:
			value_ptr = &current_.mie_scale;
			break;
		default:
			return;
		}

		float& value = *value_ptr;

		// If an external target is set, override the noise-derived target
		if (state.external_target.has_value()) {
			target = *state.external_target;
		}

		// Analytical Critically Dampened Spring
		// x(t) = (c1 + c2*t) * e^(-omega*t) + target
		// where c1 = x0 - target, c2 = v0 + omega(x0 - target)
		float x0 = value - target;
		float v0 = state.velocity;
		float omega = state.omega;

		float expTerm = std::exp(-omega * deltaTime);
		float c1 = x0;
		float c2 = v0 + omega * x0;

		value = (c1 + c2 * deltaTime) * expTerm + target;
		state.velocity = (v0 - omega * c2 * deltaTime) * expTerm;
	}

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

		UpdateAttribute(WeatherAttribute::SunIntensity, blended.sun_intensity.Lerp(sampleNoise(1.1f)), deltaTime);
		UpdateAttribute(WeatherAttribute::WindStrength, blended.wind_strength.Lerp(sampleNoise(2.2f)), deltaTime);
		UpdateAttribute(WeatherAttribute::WindSpeed, blended.wind_speed.Lerp(sampleNoise(3.3f)), deltaTime);
		UpdateAttribute(WeatherAttribute::WindFrequency, blended.wind_frequency.Lerp(sampleNoise(4.4f)), deltaTime);
		UpdateAttribute(WeatherAttribute::CloudDensity, blended.cloud_density.Lerp(sampleNoise(5.5f)), deltaTime);
		UpdateAttribute(WeatherAttribute::CloudAltitude, blended.cloud_altitude.Lerp(sampleNoise(6.6f)), deltaTime);
		UpdateAttribute(WeatherAttribute::CloudThickness, blended.cloud_thickness.Lerp(sampleNoise(7.7f)), deltaTime);
		UpdateAttribute(WeatherAttribute::HazeDensity, blended.haze_density.Lerp(sampleNoise(8.8f)), deltaTime);
		UpdateAttribute(WeatherAttribute::HazeHeight, blended.haze_height.Lerp(sampleNoise(9.9f)), deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScale, blended.rayleigh_scale.Lerp(sampleNoise(10.10f)), deltaTime);
		UpdateAttribute(WeatherAttribute::MieScale, blended.mie_scale.Lerp(sampleNoise(11.11f)), deltaTime);
	}

} // namespace Boidsish
