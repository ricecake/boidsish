#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include "weather_constants.h"
#include "weather_lbm_simulator.h"

namespace Boidsish {

	class ITerrainGenerator;

	enum class WeatherAttribute {
		SunIntensity,
		WindStrength,
		WindSpeed,
		WindFrequency,
		CloudDensity,
		CloudAltitude,
		CloudThickness,
		HazeDensity,
		HazeHeight,
		RayleighScale,
		MieScale,
		AtmosphereHeight,
		RayleighScaleHeight,
		MieScaleHeight,
		CloudCoverage,
		MieScattering,
		MieExtinction,
		RayleighScatteringR,
		RayleighScatteringG,
		RayleighScatteringB,
		HazeColorR,
		HazeColorG,
		HazeColorB,
		CloudColorR,
		CloudColorG,
		CloudColorB,
		Count
	};

	template <typename T>
	struct WeatherRange {
		T min;
		T max;

		WeatherRange operator+(const WeatherRange& other) const { return {min + other.min, max + other.max}; }

		WeatherRange operator*(float f) const { return {min * f, max * f}; }

		T Lerp(float t) const { return min + t * (max - min); }
	};

	struct WeatherSettings {
		WeatherRange<float> sun_intensity;
		WeatherRange<float> wind_strength;
		WeatherRange<float> wind_speed;
		WeatherRange<float> wind_frequency;
		WeatherRange<float> cloud_density;
		WeatherRange<float> cloud_altitude;
		WeatherRange<float> cloud_thickness;
		WeatherRange<float> haze_density;
		WeatherRange<float> haze_height;
		WeatherRange<float> rayleigh_scale;
		WeatherRange<float> mie_scale;
		WeatherRange<float> atmosphere_height;
		WeatherRange<float> rayleigh_scale_height;
		WeatherRange<float>     mie_scale_height;
		WeatherRange<float>     cloud_coverage;
		WeatherRange<float>     mie_scattering;
		WeatherRange<float>     mie_extinction;
		WeatherRange<glm::vec3> rayleigh_scattering;
		WeatherRange<glm::vec3> haze_color;
		WeatherRange<glm::vec3> cloud_color;

		WeatherSettings operator+(const WeatherSettings& other) const {
			return {
				sun_intensity + other.sun_intensity,
				wind_strength + other.wind_strength,
				wind_speed + other.wind_speed,
				wind_frequency + other.wind_frequency,
				cloud_density + other.cloud_density,
				cloud_altitude + other.cloud_altitude,
				cloud_thickness + other.cloud_thickness,
				haze_density + other.haze_density,
				haze_height + other.haze_height,
				rayleigh_scale + other.rayleigh_scale,
				mie_scale + other.mie_scale,
				atmosphere_height + other.atmosphere_height,
				rayleigh_scale_height + other.rayleigh_scale_height,
				mie_scale_height + other.mie_scale_height,
				cloud_coverage + other.cloud_coverage,
				mie_scattering + other.mie_scattering,
				mie_extinction + other.mie_extinction,
				rayleigh_scattering + other.rayleigh_scattering,
				haze_color + other.haze_color,
				cloud_color + other.cloud_color
			};
		}

		WeatherSettings operator*(float f) const {
			return {
				sun_intensity * f,
				wind_strength * f,
				wind_speed * f,
				wind_frequency * f,
				cloud_density * f,
				cloud_altitude * f,
				cloud_thickness * f,
				haze_density * f,
				haze_height * f,
				rayleigh_scale * f,
				mie_scale * f,
				atmosphere_height * f,
				rayleigh_scale_height * f,
				mie_scale_height * f,
				cloud_coverage * f,
				mie_scattering * f,
				mie_extinction * f,
				rayleigh_scattering * f,
				haze_color * f,
				cloud_color * f
			};
		}
	};

	struct WeatherPreset {
		std::string     name;
		WeatherSettings settings;
		float           weight = 1.0f;
	};

	struct WeatherBlendingInfo {
		std::string low_name;
		std::string high_name;
		float       t = 0.0f; // Interpolation between low and high
		bool        is_manual = false;
	};

	/**
	 * @brief Current weather values derived from blended ranges and noise.
	 */
	struct CurrentWeather {
		float     sun_intensity = WeatherConstants::SunIntensity.normal;
		float     wind_strength = WeatherConstants::WindStrength.normal;
		float     wind_speed = WeatherConstants::WindSpeed.normal;
		float     wind_frequency = WeatherConstants::WindFrequency.normal;
		float     cloud_density = WeatherConstants::CloudDensity.normal;
		float     cloud_altitude = WeatherConstants::CloudAltitude.normal;
		float     cloud_thickness = WeatherConstants::CloudThickness.normal;
		float     haze_density = WeatherConstants::HazeDensity.normal;
		float     haze_height = WeatherConstants::HazeHeight.normal;
		float     rayleigh_scale = WeatherConstants::RayleighScale.normal;
		float     mie_scale = WeatherConstants::MieScale.normal;
		float     atmosphere_height = WeatherConstants::AtmosphereHeight.normal;
		glm::vec3 rayleigh_scattering = WeatherConstants::RayleighScattering;
		float     mie_scattering = WeatherConstants::MieScattering;
		float     mie_extinction = WeatherConstants::MieExtinction;
		glm::vec3 ozone_absorption = WeatherConstants::OzoneAbsorption;
		float     rayleigh_scale_height = WeatherConstants::RayleighScaleHeight.normal;
		float     mie_scale_height = WeatherConstants::MieScaleHeight.normal;
		float     cloud_coverage = WeatherConstants::CloudCoverage.normal;
		glm::vec3 haze_color = WeatherConstants::DefaultHazeColor;
		glm::vec3 cloud_color = WeatherConstants::DefaultCloudColor;
	};

	class WeatherManager {
	public:
		WeatherManager(ITerrainGenerator* terrain = nullptr);
		~WeatherManager();

		void Update(float deltaTime, float totalTime, const glm::vec3& cameraPos, float timeOfDay);

		bool IsEnabled() const { return enabled_; }

		void SetEnabled(bool enabled) { enabled_ = enabled; }

		float GetTimeScale() const { return time_scale_; }

		void SetTimeScale(float scale) { time_scale_ = scale; }

		float GetSpatialScale() const { return spatial_scale_; }

		void SetSpatialScale(float scale) { spatial_scale_ = scale; }

		bool IsMacroSimEnabled() const { return macro_sim_enabled_; }
		void SetMacroSimEnabled(bool enabled) { macro_sim_enabled_ = enabled; }

		float GetSimTau() const { return lbm_simulator_ ? lbm_simulator_->GetTau() : 0.8f; }
		void  SetSimTau(float tau) {
			if (lbm_simulator_) lbm_simulator_->SetTau(tau);
		}

		void ResetMacroSim() {
			if (lbm_simulator_ && terrain_) lbm_simulator_->Reset(*terrain_);
		}

		const CurrentWeather& GetCurrentWeather() const { return current_; }
		CurrentWeather&       GetCurrentWeatherMutable() { return current_; }

		/**
		 * @brief Get information about the currently blending weather presets.
		 */
		const WeatherBlendingInfo& GetBlendingInfo() const { return blending_info_; }

		/**
		 * @brief Get names of all available weather presets.
		 */
		std::vector<std::string> GetPresetNames() const;

		/**
		 * @brief Set a manual preset index (-1 for dynamic/noise-driven).
		 */
		void SetManualPreset(int index);

		/**
		 * @brief Get the current manual preset index.
		 */
		int GetManualPreset() const { return manual_preset_idx_; }

		/**
		 * @brief Set the threshold for noise-space movement before weather targets are updated.
		 * Higher values result in longer "idling" periods.
		 */
		void SetHoldThreshold(float threshold) { hold_threshold_ = threshold; }

		float GetHoldThreshold() const { return hold_threshold_; }

		/**
		 * @brief Manually set a target level for a weather attribute.
		 * While an external target is set, the manager will not use noise-derived targets for this attribute.
		 */
		void SetTarget(WeatherAttribute attr, float target);

		/**
		 * @brief Clear an externally set target, returning the attribute to autonomous control.
		 */
		void ClearTarget(WeatherAttribute attr);

		/**
		 * @brief Set the pace (omega) of the critically dampened spring for a specific attribute.
		 * Higher values result in faster transitions.
		 */
		void SetPace(WeatherAttribute attr, float pace);

		const PhysicallyBasedWeatherOutput* GetPhysicallyBasedWeather() const {
			return lbm_simulator_ ? &lbm_simulator_->GetOutput() : nullptr;
		}

		PhysicallyBasedWeatherOutput GetWeatherAtPosition(const glm::vec3& pos) const {
			if (lbm_simulator_) return lbm_simulator_->GetWeatherAtPosition(pos);
			return PhysicallyBasedWeatherOutput{};
		}

		void UpdateWindUbo(float totalTime);

		void SetTerrainGenerator(ITerrainGenerator* terrain) { terrain_ = terrain; }

	private:
		unsigned int wind_data_ubo_ = 0;
		unsigned int wind_texture_ = 0;
		std::vector<glm::vec4> wind_data_cache_;

		struct AttributeState {
			float                velocity = 0.0f;
			float                omega = 2.0f; // Default pace
			std::optional<float> external_target;
		};

		void InitializePresets();
		void UpdateAttribute(WeatherAttribute attr, float target, float deltaTime);

		bool  enabled_ = true;
		bool  macro_sim_enabled_ = true;
		float time_scale_ = 0.005f;    // Low frequency over time
		float spatial_scale_ = 0.001f; // Low frequency over space
		float hold_threshold_ = 0.05f; // Noise-space distance threshold for updates

		std::vector<WeatherPreset> presets_;
		std::vector<float>         cdf_;
		CurrentWeather             current_;

		// Idling state
		int                 manual_preset_idx_ = -1;
		int                 last_manual_preset_idx_ = -1;
		glm::vec2           last_control_noise_{-1000.0f};
		WeatherBlendingInfo blending_info_;
		CurrentWeather      cached_targets_;

		std::array<AttributeState, static_cast<size_t>(WeatherAttribute::Count)> attribute_states_;

		ITerrainGenerator*                   terrain_ = nullptr;
		std::unique_ptr<WeatherLbmSimulator> lbm_simulator_;
	};

} // namespace Boidsish
