#include "weather_manager.h"

#include <algorithm>
#include <cmath>

#include "Simplex.h"
#include "profiler.h"
#include "service_locator.h"
#include <GL/glew.h>
#include "constants.h"
#include "NoiseManager.h"
#include "terrain_render_manager.h"

namespace Boidsish {

	WeatherManager::WeatherManager(ServiceLocator& /*loc*/): enabled_(true), terrain_(nullptr) {
		InitializePresets();

		// Initialize LBM Simulator (scaled to typical terrain range)
		lbm_simulator_ = std::make_unique<WeatherLbmSimulator>(128, 128);

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
		SetPace(WeatherAttribute::Precipitation, WeatherConstants::Precipitation.pace);
		SetPace(WeatherAttribute::Temperature, WeatherConstants::Temperature.pace);
		SetPace(WeatherAttribute::Humidity, 10.0f);
		SetPace(WeatherAttribute::Pressure, 10.0f);

		// Initialize default paces for new attributes
		SetPace(WeatherAttribute::MieScattering, 10.0f);
		SetPace(WeatherAttribute::MieExtinction, 10.0f);
		SetPace(WeatherAttribute::RayleighScatteringR, 10.0f);
		SetPace(WeatherAttribute::RayleighScatteringG, 10.0f);
		SetPace(WeatherAttribute::RayleighScatteringB, 10.0f);
		SetPace(WeatherAttribute::HazeColorR, 10.0f);
		SetPace(WeatherAttribute::HazeColorG, 10.0f);
		SetPace(WeatherAttribute::HazeColorB, 10.0f);
		SetPace(WeatherAttribute::CloudColorR, 10.0f);
		SetPace(WeatherAttribute::CloudColorG, 10.0f);
		SetPace(WeatherAttribute::CloudColorB, 10.0f);
	}

	WeatherManager::~WeatherManager() {
		if (lbm_task_.has_value()) {
			lbm_task_->cancel();
			try {
				lbm_task_->get();
			} catch (...) {}
		}

		if (wind_data_ubo_ != 0) {
			glDeleteBuffers(1, &wind_data_ubo_);
		}
		if (wind_texture_ != 0) {
			glDeleteTextures(1, &wind_texture_);
		}
		if (lbm_wind_texture_ != 0) {
			glDeleteTextures(1, &lbm_wind_texture_);
		}
		if (lbm_scalar_texture_ != 0) {
			glDeleteTextures(1, &lbm_scalar_texture_);
		}
		if (lbm_aerosol_texture_ != 0) {
			glDeleteTextures(1, &lbm_aerosol_texture_);
		}
	}

	void WeatherManager::SetTarget(WeatherAttribute attr, float target) {
		if (attr == WeatherAttribute::Count)
			return;
		auto& state = attribute_states_[static_cast<size_t>(attr)];
		state.external_target = target;

		// Snap value immediately
		float* value_ptr = GetValuePtr(attr);
		if (value_ptr) {
			*value_ptr = target;
			state.velocity = 0.0f;
		}
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

	const PhysicallyBasedWeatherOutput* WeatherManager::GetPhysicallyBasedWeather() const {
		return latest_snapshot_.valid ? &latest_snapshot_.output : nullptr;
	}

	PhysicallyBasedWeatherOutput WeatherManager::GetWeatherAtPosition(const glm::vec3& pos) const {
		if (!latest_snapshot_.valid)
			return PhysicallyBasedWeatherOutput{};

		PhysicallyBasedWeatherOutput out = latest_snapshot_.output;

		// Localize if possible using snapshot data
		int chunkX = (int)std::floor(pos.x / 32.0f);
		int chunkZ = (int)std::floor(pos.z / 32.0f);
		int x = chunkX - latest_snapshot_.gridAnchor.x;
		int z = chunkZ - latest_snapshot_.gridAnchor.y;

		int width = latest_snapshot_.uboMetadata.originSize.y;
		int height = latest_snapshot_.uboMetadata.originSize.w;

		if (x >= 0 && x < width && z >= 0 && z < height) {
			int         idx = z * width + x;
			const auto& wind = latest_snapshot_.windData[idx];
			const auto& scalars = latest_snapshot_.scalarData[idx];

			out.windVelocity = glm::vec2(wind.x, wind.z);
			out.verticalWind = wind.y;
			out.temperature = scalars.x;
			out.humidity = scalars.y;
			out.pressure = scalars.z;
		}

		return out;
	}

	void WeatherManager::InjectPressure(const glm::vec3& pos, float pressureHpa, float burstStrength) {
		std::lock_guard<std::mutex> lock(injection_mutex_);
		pending_injections_.push_back({LbmInjectionType::Pressure, pos, pressureHpa, burstStrength});
	}

	void WeatherManager::InjectAerosol(const glm::vec3& pos, float concentration) {
		std::lock_guard<std::mutex> lock(injection_mutex_);
		pending_injections_.push_back({LbmInjectionType::Aerosol, pos, concentration, 0.0f});
	}

	void WeatherManager::InjectTemperature(const glm::vec3& pos, float temperatureK) {
		std::lock_guard<std::mutex> lock(injection_mutex_);
		pending_injections_.push_back({LbmInjectionType::Temperature, pos, temperatureK, 0.0f});
	}

	void WeatherManager::UpdateAttribute(WeatherAttribute attr, float target, float deltaTime) {
		if (attr == WeatherAttribute::Count || deltaTime <= 0.0f)
			return;

		auto& state = attribute_states_[static_cast<size_t>(attr)];

		// If an external target is set, override the target and snap immediately (bypassing spring)
		if (state.external_target.has_value()) {
			target = *state.external_target;
			float* value_ptr = GetValuePtr(attr);
			if (value_ptr) {
				*value_ptr = target;
				state.velocity = 0.0f;
			}
			return;
		}

		float* value_ptr = GetValuePtr(attr);
		if (!value_ptr)
			return;

		float& value = *value_ptr;

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

	float* WeatherManager::GetValuePtr(WeatherAttribute attr) {
		switch (attr) {
		case WeatherAttribute::SunIntensity:
			return &current_.sun_intensity;
		case WeatherAttribute::WindStrength:
			return &current_.wind_strength;
		case WeatherAttribute::WindSpeed:
			return &current_.wind_speed;
		case WeatherAttribute::WindFrequency:
			return &current_.wind_frequency;
		case WeatherAttribute::CloudDensity:
			return &current_.cloud_density;
		case WeatherAttribute::CloudAltitude:
			return &current_.cloud_altitude;
		case WeatherAttribute::CloudThickness:
			return &current_.cloud_thickness;
		case WeatherAttribute::HazeDensity:
			return &current_.haze_density;
		case WeatherAttribute::HazeHeight:
			return &current_.haze_height;
		case WeatherAttribute::RayleighScale:
			return &current_.rayleigh_scale;
		case WeatherAttribute::MieScale:
			return &current_.mie_scale;
		case WeatherAttribute::AtmosphereHeight:
			return &current_.atmosphere_height;
		case WeatherAttribute::RayleighScaleHeight:
			return &current_.rayleigh_scale_height;
		case WeatherAttribute::MieScaleHeight:
			return &current_.mie_scale_height;
		case WeatherAttribute::CloudCoverage:
			return &current_.cloud_coverage;
		case WeatherAttribute::Precipitation:
			return &current_.precipitation;
		case WeatherAttribute::Temperature:
			return &current_.temperature;
		case WeatherAttribute::Humidity:
			return &current_.humidity;
		case WeatherAttribute::Pressure:
			return &current_.pressure;
		case WeatherAttribute::MieScattering:
			return &current_.mie_scattering;
		case WeatherAttribute::MieExtinction:
			return &current_.mie_extinction;
		case WeatherAttribute::RayleighScatteringR:
			return &current_.rayleigh_scattering.r;
		case WeatherAttribute::RayleighScatteringG:
			return &current_.rayleigh_scattering.g;
		case WeatherAttribute::RayleighScatteringB:
			return &current_.rayleigh_scattering.b;
		case WeatherAttribute::HazeColorR:
			return &current_.haze_color.r;
		case WeatherAttribute::HazeColorG:
			return &current_.haze_color.g;
		case WeatherAttribute::HazeColorB:
			return &current_.haze_color.b;
		case WeatherAttribute::CloudColorR:
			return &current_.cloud_color.r;
		case WeatherAttribute::CloudColorG:
			return &current_.cloud_color.g;
		case WeatherAttribute::CloudColorB:
			return &current_.cloud_color.b;
		default:
			return nullptr;
		}
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
		sunny.settings.precipitation = {0.0f, 0.0f};
		sunny.settings.temperature = {Temperature.GetValue(0.9f), Temperature.GetValue(1.1f)};
		sunny.settings.humidity = {0.2f, 0.4f};
		sunny.settings.pressure = {1010.0f, 1020.0f};
		sunny.settings.mie_scattering = {MieScattering, MieScattering};
		sunny.settings.mie_extinction = {MieExtinction, MieExtinction};
		sunny.settings.rayleigh_scattering = {RayleighScattering, RayleighScattering};
		sunny.settings.haze_color = {DefaultHazeColor, DefaultHazeColor};
		sunny.settings.cloud_color = {DefaultCloudColor, DefaultCloudColor};
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
		cloudy.settings.precipitation = {0.0f, 0.1f};
		cloudy.settings.temperature = {Temperature.GetValue(0.8f), Temperature.GetValue(1.0f)};
		cloudy.settings.humidity = {0.5f, 0.7f};
		cloudy.settings.pressure = {1000.0f, 1010.0f};
		cloudy.settings.mie_scattering = {MieScattering, MieScattering * 1.5f};
		cloudy.settings.mie_extinction = {MieExtinction, MieExtinction * 1.5f};
		cloudy.settings.rayleigh_scattering = {RayleighScattering, RayleighScattering * 1.1f};
		cloudy.settings.haze_color = {DefaultHazeColor * 0.9f, DefaultHazeColor};
		cloudy.settings.cloud_color = {DefaultCloudColor * 0.9f, DefaultCloudColor};
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
		overcast.settings.precipitation = {0.1f, 0.3f};
		overcast.settings.temperature = {Temperature.GetValue(0.7f), Temperature.GetValue(0.9f)};
		overcast.settings.humidity = {0.7f, 0.9f};
		overcast.settings.pressure = {990.0f, 1005.0f};
		overcast.settings.mie_scattering = {MieScattering * 1.5f, MieScattering * 2.0f};
		overcast.settings.mie_extinction = {MieExtinction * 1.5f, MieExtinction * 2.0f};
		overcast.settings.rayleigh_scattering = {RayleighScattering * 1.1f, RayleighScattering * 1.3f};
		overcast.settings.haze_color = {DefaultHazeColor * 0.7f, DefaultHazeColor * 0.8f};
		overcast.settings.cloud_color = {DefaultCloudColor * 0.7f, DefaultCloudColor * 0.8f};
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
		foggy.settings.precipitation = {0.0f, 0.05f};
		foggy.settings.temperature = {WeatherConstants::Temperature.GetValue(0.6f), WeatherConstants::Temperature.GetValue(0.8f)};
		foggy.settings.humidity = {0.8f, 1.0f};
		foggy.settings.pressure = {1005.0f, 1015.0f};
		foggy.settings.mie_scattering = {MieScattering * 2.0f, MieScattering * 5.0f};
		foggy.settings.mie_extinction = {MieExtinction * 2.0f, MieExtinction * 5.0f};
		foggy.settings.rayleigh_scattering = {RayleighScattering * 1.3f, RayleighScattering * 1.5f};
		foggy.settings.haze_color = {DefaultHazeColor * 0.5f, DefaultHazeColor * 0.6f};
		foggy.settings.cloud_color = {DefaultCloudColor * 0.5f, DefaultCloudColor * 0.6f};
		presets_.push_back(foggy);

		// 5. Rainy
		WeatherPreset rainy;
		rainy.name = "Rainy";
		rainy.weight = 3.0f;
		rainy.settings.sun_intensity = {SunIntensity.GetValue(0.1f), SunIntensity.GetValue(0.4f)};
		rainy.settings.wind_strength = {WindStrength.GetValue(1.5f), WindStrength.GetValue(3.0f)};
		rainy.settings.wind_speed = {WindSpeed.GetValue(1.5f), WindSpeed.GetValue(3.0f)};
		rainy.settings.wind_frequency = {WindFrequency.GetValue(1.5f), WindFrequency.GetValue(2.5f)};
		rainy.settings.cloud_density = {CloudDensity.GetValue(1.8f), CloudDensity.GetValue(2.0f)};
		rainy.settings.cloud_altitude = {CloudAltitude.GetValue(0.1f), CloudAltitude.GetValue(0.2f)};
		rainy.settings.cloud_thickness = {CloudThickness.GetValue(1.8f), CloudThickness.GetValue(2.0f)};
		rainy.settings.haze_density = {HazeDensity.GetValue(1.5f), HazeDensity.GetValue(2.0f)};
		rainy.settings.haze_height = {HazeHeight.GetValue(1.5f), HazeHeight.GetValue(2.0f)};
		rainy.settings.rayleigh_scale = {RayleighScale.GetValue(1.4f), RayleighScale.GetValue(1.8f)};
		rainy.settings.mie_scale = {MieScale.GetValue(1.5f), MieScale.GetValue(2.0f)};
		rainy.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.3f), AtmosphereHeight.GetValue(0.6f)};
		rainy.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.4f), RayleighScaleHeight.GetValue(0.8f)};
		rainy.settings.mie_scale_height = {MieScaleHeight.GetValue(1.8f), MieScaleHeight.GetValue(2.0f)};
		rainy.settings.cloud_coverage = {CloudCoverage.GetValue(1.5f), CloudCoverage.GetValue(2.0f)};
		rainy.settings.precipitation = {0.5f, 1.0f};
		rainy.settings.temperature = {WeatherConstants::Temperature.GetValue(0.7f), WeatherConstants::Temperature.GetValue(0.9f)}; // Moderate
		rainy.settings.humidity = {0.85f, 1.0f};
		rainy.settings.pressure = {980.0f, 1000.0f};
		rainy.settings.mie_scattering = {MieScattering * 1.5f, MieScattering * 2.0f};
		rainy.settings.mie_extinction = {MieExtinction * 1.5f, MieExtinction * 2.0f};
		rainy.settings.rayleigh_scattering = {RayleighScattering * 1.1f, RayleighScattering * 1.3f};
		rainy.settings.haze_color = {DefaultHazeColor * 0.6f, DefaultHazeColor * 0.7f};
		rainy.settings.cloud_color = {DefaultCloudColor * 0.6f, DefaultCloudColor * 0.7f};
		presets_.push_back(rainy);

		// 6. Snowy
		WeatherPreset snowy;
		snowy.name = "Snowy";
		snowy.weight = 2.0f;
		snowy.settings.sun_intensity = {SunIntensity.GetValue(0.1f), SunIntensity.GetValue(0.3f)};
		snowy.settings.wind_strength = {WindStrength.GetValue(1.0f), WindStrength.GetValue(2.5f)};
		snowy.settings.wind_speed = {WindSpeed.GetValue(1.0f), WindSpeed.GetValue(2.5f)};
		snowy.settings.wind_frequency = {WindFrequency.GetValue(1.0f), WindFrequency.GetValue(2.0f)};
		snowy.settings.cloud_density = {CloudDensity.GetValue(1.5f), CloudDensity.GetValue(2.0f)};
		snowy.settings.cloud_altitude = {CloudAltitude.GetValue(0.1f), CloudAltitude.GetValue(0.3f)};
		snowy.settings.cloud_thickness = {CloudThickness.GetValue(1.5f), CloudThickness.GetValue(2.0f)};
		snowy.settings.haze_density = {HazeDensity.GetValue(1.8f), HazeDensity.GetValue(2.0f)};
		snowy.settings.haze_height = {HazeHeight.GetValue(1.5f), HazeHeight.GetValue(2.0f)};
		snowy.settings.rayleigh_scale = {RayleighScale.GetValue(1.4f), RayleighScale.GetValue(1.8f)};
		snowy.settings.mie_scale = {MieScale.GetValue(1.5f), MieScale.GetValue(2.0f)};
		snowy.settings.atmosphere_height = {AtmosphereHeight.GetValue(0.3f), AtmosphereHeight.GetValue(0.6f)};
		snowy.settings.rayleigh_scale_height = {RayleighScaleHeight.GetValue(0.4f), RayleighScaleHeight.GetValue(0.8f)};
		snowy.settings.mie_scale_height = {MieScaleHeight.GetValue(1.8f), MieScaleHeight.GetValue(2.0f)};
		snowy.settings.cloud_coverage = {CloudCoverage.GetValue(1.2f), CloudCoverage.GetValue(2.0f)};
		snowy.settings.precipitation = {0.4f, 0.8f};
		snowy.settings.temperature = {WeatherConstants::Temperature.GetValue(0.2f), WeatherConstants::Temperature.GetValue(0.4f)}; // Low
		snowy.settings.humidity = {0.7f, 0.9f};
		snowy.settings.pressure = {985.0f, 1005.0f};
		snowy.settings.mie_scattering = {MieScattering * 2.0f, MieScattering * 4.0f};
		snowy.settings.mie_extinction = {MieExtinction * 2.0f, MieExtinction * 4.0f};
		snowy.settings.rayleigh_scattering = {RayleighScattering * 1.2f, RayleighScattering * 1.4f};
		snowy.settings.haze_color = {DefaultHazeColor * 0.8f, DefaultHazeColor * 0.9f};
		snowy.settings.cloud_color = {DefaultCloudColor * 0.8f, DefaultCloudColor * 0.9f};
		presets_.push_back(snowy);

		// Calculate CDF
		cdf_.clear();
		float totalWeight = 0.0f;
		for (const auto& p : presets_) {
			totalWeight += p.weight;
			cdf_.push_back(totalWeight);
		}
	}

	void WeatherManager::UpdateWindUbo(float totalTime, NoiseManager* noise, TerrainRenderManager* terrain_render) {
		if (!latest_snapshot_.valid)
			return;

		// Lazy initialization of GPU resources
		if (wind_data_ubo_ == 0) {
			glGenBuffers(1, &wind_data_ubo_);
			glBindBuffer(GL_UNIFORM_BUFFER, wind_data_ubo_);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(WindDataUbo), nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		if (wind_texture_ == 0) {
			glGenTextures(1, &wind_texture_);
			glBindTexture(GL_TEXTURE_2D, wind_texture_);
			// The integrated wind texture at higher resolution
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, 1024, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (lbm_scalar_texture_ == 0) {
			glGenTextures(1, &lbm_scalar_texture_);
			glBindTexture(GL_TEXTURE_2D, lbm_scalar_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, lbm_simulator_->GetWidth(), lbm_simulator_->GetHeight(), 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (lbm_aerosol_texture_ == 0) {
			glGenTextures(1, &lbm_aerosol_texture_);
			glBindTexture(GL_TEXTURE_2D, lbm_aerosol_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, lbm_simulator_->GetWidth(), lbm_simulator_->GetHeight(), 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (lbm_wind_texture_ == 0) {
			glGenTextures(1, &lbm_wind_texture_);
			glBindTexture(GL_TEXTURE_2D, lbm_wind_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, lbm_simulator_->GetWidth(), lbm_simulator_->GetHeight(), 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (!wind_compute_shader_) {
			wind_compute_shader_ = std::make_unique<ComputeShader>("shaders/wind_compute.comp");
		}

		WindDataUbo ubo = latest_snapshot_.uboMetadata;
		ubo.cloudAdvection = glm::vec4(cloud_advection_offset_, 0.0f, 0.0f);
		const auto& wind_data = latest_snapshot_.windData;

		if (!macro_sim_enabled_) {
			// Fallback: Uniform slowly changing wind vector
			float wind_t = totalTime * 0.05f;
			glm::vec2 windDir(
				Simplex::noise(glm::vec2(wind_t, 123.456f)),
				Simplex::noise(glm::vec2(987.654f, wind_t))
			);
			// Scale by current tuning controls
			float     conversion = 32.0f / 0.1f;
			glm::vec2 windVec = windDir * current_.wind_strength * conversion;

			// We still use the snapshot's grid dimensions for metadata but override the content
			std::vector<glm::vec4> fallback_data(wind_data.size(), glm::vec4(windVec.x, 0.0f, windVec.y, 0.0f));

			glBindBuffer(GL_UNIFORM_BUFFER, wind_data_ubo_);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(WindDataUbo), &ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::LbmWindData());
			glBindTexture(GL_TEXTURE_2D, lbm_wind_texture_);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ubo.originSize.y, ubo.originSize.w, GL_RGBA, GL_FLOAT, fallback_data.data());
		} else {
			glBindBuffer(GL_UNIFORM_BUFFER, wind_data_ubo_);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(WindDataUbo), &ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			// Update LBM Wind Texture
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::LbmWindData());
			glBindTexture(GL_TEXTURE_2D, lbm_wind_texture_);
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				ubo.originSize.y,
				ubo.originSize.w,
				GL_RGBA,
				GL_FLOAT,
				wind_data.data()
			);

			// Update LBM Scalar Texture
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WeatherScalars());
			glBindTexture(GL_TEXTURE_2D, lbm_scalar_texture_);
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				ubo.originSize.y,
				ubo.originSize.w,
				GL_RGBA,
				GL_FLOAT,
				latest_snapshot_.scalarData.data()
			);

			// Update LBM Aerosol Texture
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WeatherAerosols());
			glBindTexture(GL_TEXTURE_2D, lbm_aerosol_texture_);
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				ubo.originSize.y,
				ubo.originSize.w,
				GL_RGBA,
				GL_FLOAT,
				latest_snapshot_.aerosolData.data()
			);
		}

		// Dispatch Wind Integration Compute Shader
		wind_compute_shader_->use();

		// Bind Metadata UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::WindData(), wind_data_ubo_);

		// Bind Input LBM Wind
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::LbmWindData());
		glBindTexture(GL_TEXTURE_2D, lbm_wind_texture_);
		wind_compute_shader_->setInt("u_lbmWindTexture", Constants::TextureUnit::LbmWindData());

		// Bind Output Integrated Wind Image
		glBindImageTexture(Constants::TextureUnit::WindData(), wind_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		// Bind Dependencies (Noise & Terrain)
		if (noise) {
			noise->BindDefault(*wind_compute_shader_);
		}
		if (terrain_render) {
			terrain_render->BindTerrainData(*wind_compute_shader_);
		}

		// Dispatch
		glDispatchCompute(1024 / 16, 1024 / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		// Final Binding for users of getWindAtPosition
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WindData());
		glBindTexture(GL_TEXTURE_2D, wind_texture_);

		// Bind Scalar and Aerosol textures for general usage
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WeatherScalars());
		glBindTexture(GL_TEXTURE_2D, lbm_scalar_texture_);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WeatherAerosols());
		glBindTexture(GL_TEXTURE_2D, lbm_aerosol_texture_);
	}

	void WeatherManager::Update(float deltaTime, float totalTime, const glm::vec3& cameraPos, float timeOfDay) {
		if (!enabled_ || presets_.empty())
			return;

		PROJECT_PROFILE_SCOPE("WeatherManager::Update");

		// Accumulate delta for the LBM background task
		lbm_delta_accumulator_ += deltaTime;

		// Manage background task
		if (!lbm_task_.has_value() || lbm_task_->is_ready()) {
			if (lbm_task_.has_value()) {
				latest_snapshot_ = lbm_task_->get();
			}

			// Fire next task immediately
			float taskDelta = lbm_delta_accumulator_;
			lbm_delta_accumulator_ = 0.0f;

			// Capture parameters by value for thread safety
			float windSpeed = current_.wind_speed;
			float windStrength = current_.wind_strength;
			float windFreq = current_.wind_frequency;
			float temperature = current_.temperature;
			float pressure = current_.pressure;
			float humidity = current_.humidity;
			bool simEnabled = macro_sim_enabled_;

			lbm_task_ = lbm_pool_.enqueue(TaskPriority::MEDIUM, [this, taskDelta, totalTime, timeOfDay, cameraPos, windSpeed, windStrength, windFreq, temperature, pressure, humidity, simEnabled]() {
				if (!lbm_simulator_ || !terrain_) {
					return LbmSnapshot{};
				}

				// Handle reset request
				if (reset_requested_.exchange(false)) {
					lbm_simulator_->Reset(*terrain_, totalTime, timeOfDay);
				}

				// Handle injections
				std::vector<LbmInjection> injections;
				{
					std::lock_guard<std::mutex> lock(injection_mutex_);
					injections = std::move(pending_injections_);
					pending_injections_.clear();
				}

				for (const auto& inj : injections) {
					switch (inj.type) {
					case LbmInjectionType::Pressure:
						lbm_simulator_->InjectPressure(inj.pos, inj.value1, inj.value2);
						break;
					case LbmInjectionType::Aerosol:
						lbm_simulator_->InjectAerosol(inj.pos, inj.value1);
						break;
					case LbmInjectionType::Temperature:
						lbm_simulator_->InjectTemperature(inj.pos, inj.value1);
						break;
					}
				}

				if (simEnabled) {
					lbm_simulator_->Update(
						taskDelta,
						totalTime,
						timeOfDay,
						*terrain_,
						cameraPos,
						windSpeed,
						windStrength,
						temperature,
						pressure,
						humidity
					);
				} else {
					lbm_simulator_->UpdateAnchor(cameraPos, totalTime, timeOfDay);
				}

				LbmSnapshot snap;
				lbm_simulator_->TakeSnapshot(snap, totalTime, windFreq, 1.0f);
				return snap;
			});
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

				// If LBM is enabled, use its cloud coverage to drive weather transitions
				if (macro_sim_enabled_ && latest_snapshot_.valid) {
					controlValue = latest_snapshot_.output.cloudCoverage;
				}

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

			float humidity = 0.5f;
			float pressure = 1.0f;
			float temperature = 0.5f;
			if (macro_sim_enabled_ && latest_snapshot_.valid) {
				const auto& phys = latest_snapshot_.output;
				humidity = phys.humidity;
				pressure = std::clamp((phys.pressure - 950.0f) / 100.0f, 0.0f, 1.0f);
				temperature = std::clamp((phys.temperature - 250.0f) / 50.0f, 0.0f, 1.0f);
			}

			cached_targets_.sun_intensity = blended.sun_intensity.Lerp(sampleNoise(1.1f) * (1.0f - humidity * 0.5f));
			cached_targets_.wind_strength = blended.wind_strength.Lerp(sampleNoise(2.2f));
			cached_targets_.wind_speed = blended.wind_speed.Lerp(sampleNoise(3.3f));
			cached_targets_.wind_frequency = blended.wind_frequency.Lerp(sampleNoise(4.4f));
			cached_targets_.cloud_density = blended.cloud_density.Lerp(humidity);
			cached_targets_.cloud_altitude = blended.cloud_altitude.Lerp(sampleNoise(6.6f));
			cached_targets_.cloud_thickness = blended.cloud_thickness.Lerp(humidity);
			cached_targets_.haze_density = blended.haze_density.Lerp(humidity);
			cached_targets_.haze_height = blended.haze_height.Lerp(sampleNoise(9.9f));
			cached_targets_.rayleigh_scale = blended.rayleigh_scale.Lerp(pressure);
			cached_targets_.mie_scale = blended.mie_scale.Lerp(humidity);
			cached_targets_.atmosphere_height = blended.atmosphere_height.Lerp(pressure);
			cached_targets_.rayleigh_scale_height = blended.rayleigh_scale_height.Lerp(temperature);
			cached_targets_.mie_scale_height = blended.mie_scale_height.Lerp(humidity);
			cached_targets_.cloud_coverage = blended.cloud_coverage.Lerp(humidity);
			cached_targets_.precipitation = blended.precipitation.Lerp(humidity);
			cached_targets_.temperature = blended.temperature.Lerp(temperature);
			cached_targets_.humidity = blended.humidity.Lerp(humidity);
			cached_targets_.pressure = blended.pressure.Lerp(pressure);

			cached_targets_.mie_scattering = blended.mie_scattering.Lerp(humidity);
			cached_targets_.mie_extinction = blended.mie_extinction.Lerp(humidity);
			cached_targets_.rayleigh_scattering = blended.rayleigh_scattering.Lerp(pressure);
			cached_targets_.haze_color = blended.haze_color.Lerp(humidity);
			cached_targets_.cloud_color = blended.cloud_color.Lerp(humidity);
		}

		// If LBM is enabled, some targets are driven directly by simulation
		if (macro_sim_enabled_ && latest_snapshot_.valid) {
			const auto& phys = latest_snapshot_.output;
			cached_targets_.rayleigh_scattering = phys.rayleighScattering;
			cached_targets_.mie_scattering = phys.mieScattering;
			cached_targets_.mie_extinction = phys.mieExtinction;
			cached_targets_.haze_color = phys.aerosolColor; // LBM aerosol color maps to haze color
			cached_targets_.cloud_coverage = phys.cloudCoverage;
			cached_targets_.cloud_density = phys.cloudDensity;
			cached_targets_.cloud_altitude = phys.cloudAltitude;
			cached_targets_.cloud_thickness = phys.cloudThickness;
			cached_targets_.temperature = phys.temperature;
			cached_targets_.pressure = phys.pressure;
			cached_targets_.humidity = phys.humidity;
			cached_targets_.wind_strength = glm::length(phys.windVelocity);

			// Precipitation heuristic: High humidity + updrafts + clouds
			float precip = std::max(0.0f, phys.humidity - 0.8f) * 5.0f;
			precip *= phys.cloudCoverage;
			cached_targets_.precipitation = std::clamp(precip, 0.0f, 1.0f);
		}

		// Update cloud advection based on current wind
		// We use a simplified power-law scaling for cloud altitude: v_cloud = v_ground * (h_cloud / 1.0)^0.14
		float cloud_h = std::max(1.0f, current_.cloud_altitude);
		float cloud_wind_multiplier = std::pow(cloud_h, 0.14f);
		glm::vec2 wind_vel(0.0f);
		if (macro_sim_enabled_ && latest_snapshot_.valid) {
			wind_vel = latest_snapshot_.output.windVelocity;
		} else {
			// Fallback: Uniform slowly changing wind vector (matching UpdateWindUbo fallback)
			float wind_t = totalTime * 0.05f;
			glm::vec2 windDir(
				Simplex::noise(glm::vec2(wind_t, 123.456f)),
				Simplex::noise(glm::vec2(987.654f, wind_t))
			);
			wind_vel = windDir * current_.wind_strength * (32.0f / 0.1f);
		}
		cloud_advection_offset_ += wind_vel * cloud_wind_multiplier * deltaTime;

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
		UpdateAttribute(WeatherAttribute::Precipitation, cached_targets_.precipitation, deltaTime);
		UpdateAttribute(WeatherAttribute::Temperature, cached_targets_.temperature, deltaTime);
		UpdateAttribute(WeatherAttribute::Humidity, cached_targets_.humidity, deltaTime);
		UpdateAttribute(WeatherAttribute::Pressure, cached_targets_.pressure, deltaTime);

		// Update new attributes
		UpdateAttribute(WeatherAttribute::MieScattering, cached_targets_.mie_scattering, deltaTime);
		UpdateAttribute(WeatherAttribute::MieExtinction, cached_targets_.mie_extinction, deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScatteringR, cached_targets_.rayleigh_scattering.r, deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScatteringG, cached_targets_.rayleigh_scattering.g, deltaTime);
		UpdateAttribute(WeatherAttribute::RayleighScatteringB, cached_targets_.rayleigh_scattering.b, deltaTime);
		UpdateAttribute(WeatherAttribute::HazeColorR, cached_targets_.haze_color.r, deltaTime);
		UpdateAttribute(WeatherAttribute::HazeColorG, cached_targets_.haze_color.g, deltaTime);
		UpdateAttribute(WeatherAttribute::HazeColorB, cached_targets_.haze_color.b, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudColorR, cached_targets_.cloud_color.r, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudColorG, cached_targets_.cloud_color.g, deltaTime);
		UpdateAttribute(WeatherAttribute::CloudColorB, cached_targets_.cloud_color.b, deltaTime);
	}

} // namespace Boidsish
