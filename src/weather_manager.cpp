#include "weather_manager.h"

#include <algorithm>
#include <cmath>

#include "Simplex.h"
#include "profiler.h"

namespace Boidsish {

	WeatherManager::WeatherManager(): enabled_(true) {
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
		SetPace(WeatherAttribute::AtmosphereHeight, 0.05f);
		SetPace(WeatherAttribute::RayleighScaleHeight, 0.05f);
		SetPace(WeatherAttribute::MieScaleHeight, 0.05f);
		SetPace(WeatherAttribute::CloudCoverage, 0.2f);
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

	std::vector<std::string> WeatherManager::GetPresetNames() const {
		std::vector<std::string> names;
		for (const auto& p : presets_) {
			names.push_back(p.name);
		}
		return names;
	}

	void WeatherManager::SetManualPreset(int index) {
		if (index < -1 || index >= (int)presets_.size())
			return;
		manual_preset_idx_ = index;
		// Force update on next frame
		last_control_noise_ = glm::vec2(-1000.0f);
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
		case WeatherAttribute::AtmosphereHeight:
			value_ptr = &current_.atmosphere_height;
			break;
		case WeatherAttribute::RayleighScaleHeight:
			value_ptr = &current_.rayleigh_scale_height;
			break;
		case WeatherAttribute::MieScaleHeight:
			value_ptr = &current_.mie_scale_height;
			break;
		case WeatherAttribute::CloudCoverage:
			value_ptr = &current_.cloud_coverage;
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
		// Initialize cached targets to default sunny values to prevent initial transition
		cached_targets_ = current_;

		presets_.clear();
		// 1. Sunny
		WeatherPreset sunny;
		sunny.name = "Sunny";
		sunny.weight = 10.0f;
		sunny.settings.sun_intensity = {0.9f, 1.10f};
		sunny.settings.wind_strength = {0.02f, 0.08f};
		sunny.settings.wind_speed = {0.05f, 0.10f};
		sunny.settings.wind_frequency = {0.01f, 0.02f};
		sunny.settings.cloud_density = {0.18f, 0.22f};
		sunny.settings.cloud_altitude = {380.0f, 420.0f};
		sunny.settings.cloud_thickness = {180.0f, 220.0f};
		sunny.settings.haze_density = {0.002f, 0.004f};
		sunny.settings.haze_height = {15.0f, 25.0f};
		sunny.settings.rayleigh_scale = {1.0f, 1.2f};
		sunny.settings.mie_scale = {0.2f, 0.4f};
		sunny.settings.atmosphere_height = {110.0f, 130.0f};
		sunny.settings.rayleigh_scale_height = {7.0f, 9.0f};
		sunny.settings.mie_scale_height = {1.0f, 1.4f};
		sunny.settings.cloud_coverage = {0.70f, 0.80f};
		presets_.push_back(sunny);

		// 2. Cloudy
		WeatherPreset cloudy;
		cloudy.name = "Cloudy";
		cloudy.weight = 6.0f;
		cloudy.settings.sun_intensity = {0.50f, 0.80f};
		cloudy.settings.wind_strength = {0.1f, 0.2f};
		cloudy.settings.wind_speed = {0.2f, 0.4f};
		cloudy.settings.wind_frequency = {0.02f, 0.05f};
		cloudy.settings.cloud_density = {0.30f, 0.50f};
		cloudy.settings.cloud_altitude = {250.0f, 350.0f};
		cloudy.settings.cloud_thickness = {300.0f, 500.0f};
		cloudy.settings.haze_density = {0.005f, 0.010f};
		cloudy.settings.haze_height = {30.0f, 60.0f};
		cloudy.settings.rayleigh_scale = {1.2f, 1.5f};
		cloudy.settings.mie_scale = {0.4f, 0.8f};
		cloudy.settings.atmosphere_height = {80.0f, 110.0f};
		cloudy.settings.rayleigh_scale_height = {6.0f, 8.0f};
		cloudy.settings.mie_scale_height = {1.5f, 2.5f};
		cloudy.settings.cloud_coverage = {0.80f, 0.90f};
		presets_.push_back(cloudy);

		// 3. Overcast
		WeatherPreset overcast;
		overcast.name = "Overcast";
		overcast.weight = 4.0f;
		overcast.settings.sun_intensity = {0.20f, 0.50f};
		overcast.settings.wind_strength = {0.2f, 0.5f};
		overcast.settings.wind_speed = {0.4f, 0.8f};
		overcast.settings.wind_frequency = {0.04f, 0.08f};
		overcast.settings.cloud_density = {0.60f, 1.00f};
		overcast.settings.cloud_altitude = {100.0f, 200.0f};
		overcast.settings.cloud_thickness = {500.0f, 800.0f};
		overcast.settings.haze_density = {0.01f, 0.02f};
		overcast.settings.haze_height = {50.0f, 100.0f};
		overcast.settings.rayleigh_scale = {1.5f, 2.0f};
		overcast.settings.mie_scale = {0.5f, 1.0f};
		overcast.settings.atmosphere_height = {50.0f, 80.0f};
		overcast.settings.rayleigh_scale_height = {5.0f, 7.0f};
		overcast.settings.mie_scale_height = {3.0f, 5.0f};
		overcast.settings.cloud_coverage = {0.90f, 1.00f};
		presets_.push_back(overcast);

		// 4. Foggy
		WeatherPreset foggy;
		foggy.name = "Foggy";
		foggy.weight = 2.0f;
		foggy.settings.sun_intensity = {0.10f, 0.30f};
		foggy.settings.wind_strength = {0.01f, 0.05f};
		foggy.settings.wind_speed = {0.01f, 0.1f};
		foggy.settings.wind_frequency = {0.005f, 0.02f};
		foggy.settings.cloud_density = {0.20f, 0.40f};
		foggy.settings.cloud_altitude = {150.0f, 250.0f};
		foggy.settings.cloud_thickness = {50.0f, 150.0f};
		foggy.settings.haze_density = {0.03f, 0.06f};
		foggy.settings.haze_height = {80.0f, 150.0f};
		foggy.settings.rayleigh_scale = {2.0f, 3.0f};
		foggy.settings.mie_scale = {1.5f, 4.0f};
		foggy.settings.atmosphere_height = {30.0f, 50.0f};
		foggy.settings.rayleigh_scale_height = {4.0f, 6.0f};
		foggy.settings.mie_scale_height = {5.0f, 10.0f};
		foggy.settings.cloud_coverage = {0.3f, 0.5f};
		presets_.push_back(foggy);

		// Calculate CDF
		cdf_.clear();
		float totalWeight = 0.0f;
		for (const auto& p : presets_) {
			totalWeight += p.weight;
			cdf_.push_back(totalWeight);
		}
	}

	void WeatherManager::Update(float deltaTime, float totalTime, const glm::vec3& cameraPos) {
		if (!enabled_ || presets_.empty())
			return;

		PROJECT_PROFILE_SCOPE("WeatherManager::Update");

		// Calculate weather control coordinate in noise-space
		glm::vec2 noisePos = glm::vec2(cameraPos.x, cameraPos.z) * spatial_scale_ + glm::vec2(totalTime * time_scale_);

		// Check if we need to update the targets (idling logic)
		float dist = glm::distance(noisePos, last_control_noise_);
		bool  manual_changed = (manual_preset_idx_ != last_manual_preset_idx_);
		if (dist > hold_threshold_ || manual_changed) {
			last_control_noise_ = noisePos;
			last_manual_preset_idx_ = manual_preset_idx_;

			float t = 0.0f;
			int   lowIdx = 0;
			int   highIdx = 0;

			if (manual_preset_idx_ != -1) {
				lowIdx = manual_preset_idx_;
				highIdx = manual_preset_idx_;
				t = 0.0f;
				blending_info_.is_manual = true;
			} else {
				float controlValue = Simplex::noise(noisePos) * 0.5f + 0.5f;
				float totalWeight = cdf_.back();
				float targetValue = std::clamp(controlValue, 0.0f, 1.0f) * totalWeight;

				auto it = std::upper_bound(cdf_.begin(), cdf_.end(), targetValue);
				highIdx = std::distance(cdf_.begin(), it);
				lowIdx = std::max(0, highIdx - 1);
				highIdx = std::min(highIdx, (int)presets_.size() - 1);

				float weightHigh = cdf_[highIdx];
				float weightLow = (highIdx == 0) ? 0.0f : cdf_[highIdx - 1];
				float segmentWidth = weightHigh - weightLow;
				t = (segmentWidth > 0.0001f) ? (targetValue - weightLow) / segmentWidth : 0.0f;

				blending_info_.is_manual = false;
			}

			blending_info_.low_name = presets_[lowIdx].name;
			blending_info_.high_name = presets_[highIdx].name;
			blending_info_.t = t;

			const auto& lowPreset = presets_[lowIdx].settings;
			const auto& highPreset = presets_[highIdx].settings;

			// Interpolated settings (blended ranges)
			WeatherSettings blended = lowPreset * (1.0f - t) + highPreset * t;

			// Use secondary noise to pick values within the blended ranges
			auto sampleNoise = [&](float offset) {
				return Simplex::noise(noisePos * 2.0f + glm::vec2(offset)) * 0.5f + 0.5f;
			};

			cached_targets_.sun_intensity = blended.sun_intensity.Lerp(sampleNoise(1.1f));
			cached_targets_.wind_strength = blended.wind_strength.Lerp(sampleNoise(2.2f));
			cached_targets_.wind_speed = blended.wind_speed.Lerp(sampleNoise(3.3f));
			cached_targets_.wind_frequency = blended.wind_frequency.Lerp(sampleNoise(4.4f));
			cached_targets_.cloud_density = blended.cloud_density.Lerp(sampleNoise(5.5f));
			cached_targets_.cloud_altitude = blended.cloud_altitude.Lerp(sampleNoise(6.6f));
			cached_targets_.cloud_thickness = blended.cloud_thickness.Lerp(sampleNoise(7.7f));
			cached_targets_.haze_density = blended.haze_density.Lerp(sampleNoise(8.8f));
			cached_targets_.haze_height = blended.haze_height.Lerp(sampleNoise(9.9f));
			cached_targets_.rayleigh_scale = blended.rayleigh_scale.Lerp(sampleNoise(10.10f));
			cached_targets_.mie_scale = blended.mie_scale.Lerp(sampleNoise(11.11f));
			cached_targets_.atmosphere_height = blended.atmosphere_height.Lerp(sampleNoise(12.12f));
			cached_targets_.rayleigh_scale_height = blended.rayleigh_scale_height.Lerp(sampleNoise(13.13f));
			cached_targets_.mie_scale_height = blended.mie_scale_height.Lerp(sampleNoise(14.14f));
			cached_targets_.cloud_coverage = blended.cloud_coverage.Lerp(sampleNoise(15.15f));
		}

		// Always update attributes toward cached targets using the spring system
		UpdateAttribute(WeatherAttribute::SunIntensity, cached_targets_.sun_intensity, deltaTime);
		UpdateAttribute(WeatherAttribute::WindStrength, cached_targets_.wind_strength, deltaTime);
		UpdateAttribute(WeatherAttribute::WindSpeed, cached_targets_.wind_speed, deltaTime);
		UpdateAttribute(WeatherAttribute::WindFrequency, cached_targets_.wind_frequency, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudDensity, cached_targets_.cloud_density, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudAltitude, cached_targets_.cloud_altitude, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudThickness, cached_targets_.cloud_thickness, deltaTime);
		UpdateAttribute(WeatherAttribute::HazeDensity, cached_targets_.haze_density, deltaTime);
		UpdateAttribute(WeatherAttribute::HazeHeight, cached_targets_.haze_height, deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScale, cached_targets_.rayleigh_scale, deltaTime);
		UpdateAttribute(WeatherAttribute::MieScale, cached_targets_.mie_scale, deltaTime);
		UpdateAttribute(WeatherAttribute::AtmosphereHeight, cached_targets_.atmosphere_height, deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScaleHeight, cached_targets_.rayleigh_scale_height, deltaTime);
		UpdateAttribute(WeatherAttribute::MieScaleHeight, cached_targets_.mie_scale_height, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudCoverage, cached_targets_.cloud_coverage, deltaTime);
	}

} // namespace Boidsish
