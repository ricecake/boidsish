#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace Boidsish {

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
				mie_scale + other.mie_scale
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
				mie_scale * f
			};
		}
	};

	struct WeatherPreset {
		std::string     name;
		WeatherSettings settings;
		float           weight = 1.0f;
	};

	/**
	 * @brief Current weather values derived from blended ranges and noise.
	 */
	struct CurrentWeather {
		float sun_intensity = 1.0f;
		float wind_strength = 0.065f;
		float wind_speed = 0.075f;
		float wind_frequency = 0.01f;
		float cloud_density = 0.5f;
		float cloud_altitude = 175.0f;
		float cloud_thickness = 10.0f;
		float haze_density = 0.003f;
		float haze_height = 20.0f;
		float rayleigh_scale = 1.0f;
		float mie_scale = 0.1f;
	};

	class WeatherManager {
	public:
		WeatherManager();
		~WeatherManager();

		void Update(float deltaTime, float totalTime, const glm::vec3& cameraPos);

		bool IsEnabled() const { return enabled_; }

		void SetEnabled(bool enabled) { enabled_ = enabled; }

		float GetTimeScale() const { return time_scale_; }

		void SetTimeScale(float scale) { time_scale_ = scale; }

		float GetSpatialScale() const { return spatial_scale_; }

		void SetSpatialScale(float scale) { spatial_scale_ = scale; }

		const CurrentWeather& GetCurrentWeather() const { return current_; }

	private:
		void InitializePresets();

		bool  enabled_ = true;
		float time_scale_ = 0.005f;     // Low frequency over time
		float spatial_scale_ = 0.001f; // Low frequency over space

		std::vector<WeatherPreset> presets_;
		CurrentWeather             current_;
	};

} // namespace Boidsish
