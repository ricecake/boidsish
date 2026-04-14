#include "weather_manager.h"

#include <algorithm>
#include <cmath>

#include "Simplex.h"
#include "profiler.h"

namespace Boidsish {

	WeatherManager::WeatherManager(): enabled_(true) {
		InitializePresets();

		// Initialize simulation (e.g., 256x256 grid with 1.0m cell size)
		simulation_ = std::make_unique<WeatherSimulationManager>(256, 256, 1.0f);

		// Initialize default paces for various attributes from centralized constants
		SetPace(WeatherAttribute::SunIntensity, WeatherConstants::SunIntensity.pace);
		SetPace(WeatherAttribute::WindStrength, WeatherConstants::WindStrength.pace);
		SetPace(WeatherAttribute::WindSpeed, WeatherConstants::WindSpeed.pace);
		SetPace(WeatherAttribute::WindFrequency, WeatherConstants::WindFrequency.pace);
		SetPace(WeatherAttribute::CloudDensity, WeatherConstants::CloudDensity.pace);
		SetPace(WeatherAttribute::CloudAltitude, WeatherConstants::CloudAltitude.pace);
		SetPace(WeatherAttribute::CloudThickness, WeatherConstants::CloudThickness.pace);
		SetPace(WeatherAttribute::HazeDensity, WeatherConstants::HazeDensity.pace);
		SetPace(WeatherAttribute::HazeHeight, WeatherConstants::HazeHeight.pace);
		SetPace(WeatherAttribute::RayleighScale, WeatherConstants::RayleighScale.pace);
		SetPace(WeatherAttribute::MieScale, WeatherConstants::MieScale.pace);
		SetPace(WeatherAttribute::AtmosphereHeight, WeatherConstants::AtmosphereHeight.pace);
		SetPace(WeatherAttribute::RayleighScaleHeight, WeatherConstants::RayleighScaleHeight.pace);
		SetPace(WeatherAttribute::MieScaleHeight, WeatherConstants::MieScaleHeight.pace);
		SetPace(WeatherAttribute::CloudCoverage, WeatherConstants::CloudCoverage.pace);
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
		// Initialize cached targets to default values to prevent initial transition
		cached_targets_ = current_;

		using namespace WeatherConstants;

		presets_.clear();
		// 1. Sunny
		WeatherPreset sunny;
		sunny.name = "Sunny";
		sunny.weight = 10.0f;
		sunny.settings.sun_intensity = {SunIntensity.GetValue(0.9f), SunIntensity.GetValue(1.1f)};
		sunny.settings.wind_strength = {WindStrength.GetValue(0.4f), WindStrength.GetValue(1.6f)};
		sunny.settings.wind_speed = {WindSpeed.GetValue(0.6f), WindSpeed.GetValue(1.3f)};
		sunny.settings.wind_frequency = {WindFrequency.GetValue(1.0f), WindFrequency.GetValue(2.0f)};
		sunny.settings.cloud_density = {CloudDensity.GetValue(0.9f), CloudDensity.GetValue(1.1f)};
		sunny.settings.cloud_altitude = {CloudAltitude.GetValue(0.95f), CloudAltitude.GetValue(1.05f)};
		sunny.settings.cloud_thickness = {CloudThickness.GetValue(0.9f), CloudThickness.GetValue(1.1f)};
		sunny.settings.haze_density = {HazeDensity.GetValue(0.6f), HazeDensity.GetValue(1.3f)};
		sunny.settings.haze_height = {HazeHeight.GetValue(0.75f), HazeHeight.GetValue(1.25f)};
		sunny.settings.rayleigh_scale = {RayleighScale.GetValue(0.9f), RayleighScale.GetValue(1.1f)};
		sunny.settings.mie_scale = {MieScale.GetValue(0.6f), MieScale.GetValue(1.1f)};
		sunny.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.9f), AtmosphereHeight.GetValue(1.1f)};
		sunny.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.85f), RayleighScaleHeight.GetValue(1.15f)};
		sunny.settings.mie_scale_height = {MieScaleHeight.GetValue(0.8f), MieScaleHeight.GetValue(1.2f)};
		sunny.settings.cloud_coverage = {CloudCoverage.GetValue(0.93f), CloudCoverage.GetValue(1.07f)};
		presets_.push_back(sunny);

		// 2. Cloudy
		WeatherPreset cloudy;
		cloudy.name = "Cloudy";
		cloudy.weight = 6.0f;
		cloudy.settings.sun_intensity = {SunIntensity.GetValue(0.5f), SunIntensity.GetValue(0.8f)};
		cloudy.settings.wind_strength = {WindStrength.GetValue(1.5f), WindStrength.GetValue(2.0f)};
		cloudy.settings.wind_speed = {WindSpeed.GetValue(1.5f), WindSpeed.GetValue(2.0f)};
		cloudy.settings.wind_frequency = {WindFrequency.GetValue(1.2f), WindFrequency.GetValue(1.8f)};
		cloudy.settings.cloud_density = {CloudDensity.GetValue(1.5f), CloudDensity.GetValue(2.0f)};
		cloudy.settings.cloud_altitude = {CloudAltitude.GetValue(0.5f), CloudAltitude.GetValue(0.8f)};
		cloudy.settings.cloud_thickness = {CloudThickness.GetValue(1.2f), CloudThickness.GetValue(1.8f)};
		cloudy.settings.haze_density = {HazeDensity.GetValue(1.5f), HazeDensity.GetValue(1.8f)};
		cloudy.settings.haze_height = {HazeHeight.GetValue(1.2f), HazeHeight.GetValue(1.6f)};
		cloudy.settings.rayleigh_scale = {RayleighScale.GetValue(1.1f), RayleighScale.GetValue(1.4f)};
		cloudy.settings.mie_scale = {MieScale.GetValue(1.1f), MieScale.GetValue(1.8f)};
		cloudy.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.6f), AtmosphereHeight.GetValue(0.9f)};
		cloudy.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.7f), RayleighScaleHeight.GetValue(1.0f)};
		cloudy.settings.mie_scale_height = {MieScaleHeight.GetValue(1.2f), MieScaleHeight.GetValue(1.8f)};
		cloudy.settings.cloud_coverage = {CloudCoverage.GetValue(1.1f), CloudCoverage.GetValue(1.2f)};
		presets_.push_back(cloudy);

		// 3. Overcast
		WeatherPreset overcast;
		overcast.name = "Overcast";
		overcast.weight = 4.0f;
		overcast.settings.sun_intensity = {SunIntensity.GetValue(0.2f), SunIntensity.GetValue(0.5f)};
		overcast.settings.wind_strength = {WindStrength.GetValue(1.8f), WindStrength.GetValue(2.0f)};
		overcast.settings.wind_speed = {WindSpeed.GetValue(1.8f), WindSpeed.GetValue(2.0f)};
		overcast.settings.wind_frequency = {WindFrequency.GetValue(1.5f), WindFrequency.GetValue(2.0f)};
		overcast.settings.cloud_density = {CloudDensity.GetValue(1.8f), CloudDensity.GetValue(2.0f)};
		overcast.settings.cloud_altitude = {CloudAltitude.GetValue(0.1f), CloudAltitude.GetValue(0.3f)};
		overcast.settings.cloud_thickness = {CloudThickness.GetValue(1.8f), CloudThickness.GetValue(2.0f)};
		overcast.settings.haze_density = {HazeDensity.GetValue(1.8f), HazeDensity.GetValue(2.0f)};
		overcast.settings.haze_height = {HazeHeight.GetValue(1.5f), HazeHeight.GetValue(2.0f)};
		overcast.settings.rayleigh_scale = {RayleighScale.GetValue(1.4f), RayleighScale.GetValue(1.8f)};
		overcast.settings.mie_scale = {MieScale.GetValue(1.5f), MieScale.GetValue(2.0f)};
		overcast.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.3f), AtmosphereHeight.GetValue(0.6f)};
		overcast.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.4f), RayleighScaleHeight.GetValue(0.8f)};
		overcast.settings.mie_scale_height = {MieScaleHeight.GetValue(1.8f), MieScaleHeight.GetValue(2.0f)};
		overcast.settings.cloud_coverage = {CloudCoverage.GetValue(1.2f), CloudCoverage.GetValue(2.0f)};
		presets_.push_back(overcast);

		// 4. Foggy
		WeatherPreset foggy;
		foggy.name = "Foggy";
		foggy.weight = 2.0f;
		foggy.settings.sun_intensity = {SunIntensity.GetValue(0.1f), SunIntensity.GetValue(0.3f)};
		foggy.settings.wind_strength = {WindStrength.GetValue(0.1f), WindStrength.GetValue(0.5f)};
		foggy.settings.wind_speed = {WindSpeed.GetValue(0.1f), WindSpeed.GetValue(0.5f)};
		foggy.settings.wind_frequency = {WindFrequency.GetValue(0.5f), WindFrequency.GetValue(1.0f)};
		foggy.settings.cloud_density = {CloudDensity.GetValue(1.0f), CloudDensity.GetValue(1.5f)};
		foggy.settings.cloud_altitude = {CloudAltitude.GetValue(0.2f), CloudAltitude.GetValue(0.5f)};
		foggy.settings.cloud_thickness = {CloudThickness.GetValue(0.1f), CloudThickness.GetValue(0.5f)};
		foggy.settings.haze_density = {HazeDensity.GetValue(2.0f), HazeDensity.GetValue(2.0f)}; // High density
		foggy.settings.haze_height = {HazeHeight.GetValue(1.8f), HazeHeight.GetValue(2.0f)};
		foggy.settings.rayleigh_scale = {RayleighScale.GetValue(1.8f), RayleighScale.GetValue(2.0f)};
		foggy.settings.mie_scale = {MieScale.GetValue(1.8f), MieScale.GetValue(2.0f)};
		foggy.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.1f), AtmosphereHeight.GetValue(0.3f)};
		foggy.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.1f), RayleighScaleHeight.GetValue(0.5f)};
		foggy.settings.mie_scale_height = {MieScaleHeight.GetValue(2.0f), MieScaleHeight.GetValue(2.0f)};
		foggy.settings.cloud_coverage = {CloudCoverage.GetValue(0.2f), CloudCoverage.GetValue(0.5f)};
		presets_.push_back(foggy);

		// Calculate CDF
		cdf_.clear();
		float totalWeight = 0.0f;
		for (const auto& p : presets_) {
			totalWeight += p.weight;
			cdf_.push_back(totalWeight);
		}
	}

	void WeatherManager::Update(
		float                       deltaTime,
		float                       totalTime,
		const glm::vec3&            cameraPos,
		float                       dayNightTemperature,
		const TerrainRenderManager* terrainRenderManager
	) {
		if (!enabled_ || presets_.empty())
			return;

		PROJECT_PROFILE_SCOPE("WeatherManager::Update");

		// Update LBM simulation with temperature from day/night cycle
		if (simulation_) {
			simulation_->Update(deltaTime, dayNightTemperature, aerosol_sources_, terrainRenderManager);
		}

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

	void WeatherManager::AddAerosolSource(const AerosolSource& source) {
		aerosol_sources_.push_back(source);
	}

} // namespace Boidsish
