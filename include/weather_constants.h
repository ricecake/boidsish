#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

	struct WeatherAttributeInfo {
		float min;
		float normal;
		float max;
		float pace;

		constexpr float GetValue(float factor) const {
			if (factor <= 1.0f) {
				return min + factor * (normal - min);
			} else {
				return normal + (factor - 1.0f) * (max - normal);
			}
		}
	};

	namespace WeatherConstants {
		// Paces and Ranges
		constexpr WeatherAttributeInfo SunIntensity = {1.0f, 1.0f, 1.0f, 0.5f};
		constexpr WeatherAttributeInfo WindStrength = {0.01f, 0.05f, 0.5f, 0.13f};
		constexpr WeatherAttributeInfo WindSpeed = {0.01f, 0.075f, 0.8f, 0.13f};
		constexpr WeatherAttributeInfo WindFrequency = {0.005f, 0.01f, 0.08f, 0.05f};
		constexpr WeatherAttributeInfo CloudDensity = {0.1f, 0.20f, 1.0f, 0.2f};
		constexpr WeatherAttributeInfo CloudAltitude = {100.0f, 400.0f, 800.0f, 0.1f};
		constexpr WeatherAttributeInfo CloudThickness = {50.0f, 200.0f, 800.0f, 0.1f};
		constexpr WeatherAttributeInfo HazeDensity = {0.0f, 1.0f, 10.0f, 0.2f};
		constexpr WeatherAttributeInfo HazeHeight = {10.0f, 20.0f, 150.0f, 0.1f};
		constexpr WeatherAttributeInfo RayleighScale = {0.5f, 1.1f, 3.0f, 0.1f};
		constexpr WeatherAttributeInfo MieScale = {0.05f, 0.35f, 4.0f, 0.1f};
		constexpr WeatherAttributeInfo AtmosphereHeight = {30.0f, 120.0f, 150.0f, 0.05f};
		constexpr WeatherAttributeInfo RayleighScaleHeight = {4.0f, 8.0f, 10.0f, 0.05f};
		constexpr WeatherAttributeInfo MieScaleHeight = {1.0f, 1.2f, 10.0f, 0.05f};
		constexpr WeatherAttributeInfo CloudCoverage = {0.0f, 0.85f, 1.0f, 0.2f};

		// Fixed Atmosphere Constants (Canonical)
		inline const glm::vec3 RayleighScattering = glm::vec3(5.802f, 13.558f, 33.100f) * 1e-3f;
		constexpr float       MieScattering = 3.996f * 1e-3f;
		constexpr float       MieExtinction = 4.440f * 1e-3f;
		inline const glm::vec3 OzoneAbsorption = glm::vec3(0.650f, 1.881f, 0.085f) * 1e-3f;
		constexpr float       MieAnisotropy = 0.8f;

		// Default Cloud/Haze Colors
		inline const glm::vec3 DefaultHazeColor = glm::vec3(0.6f, 0.7f, 0.8f);
		inline const glm::vec3 DefaultCloudColor = glm::vec3(0.95f, 0.95f, 1.0f);
	} // namespace WeatherConstants

} // namespace Boidsish
