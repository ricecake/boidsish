#ifndef ATMOSPHERE_MANAGER_H
#define ATMOSPHERE_MANAGER_H

#include <memory>
#include <vector>

#include "IManager.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "weather_constants.h"

class ComputeShader;
class ShaderBase;

namespace Boidsish {

	class ServiceLocator;

	class AtmosphereManager: public IManager {
	public:
		AtmosphereManager(ServiceLocator& loc);
		~AtmosphereManager();

		void Initialize() override;
		void Update(
			const glm::vec3& sunDir,
			const glm::vec3& sunColor,
			float            sunIntensity,
			const glm::vec3& cameraPos,
			float            time,
			float            worldScale
		);

		glm::vec3 GetAmbientEstimate() const { return _ambientEstimate; }

		GLuint GetTransmittanceLUT() const { return _transmittanceLUT; }

		GLuint GetMultiScatteringLUT() const { return _multiScatteringLUT; }

		GLuint GetSkyViewLUT() const { return _skyViewLUT; }

		GLuint GetAerialPerspectiveLUT() const { return _aerialPerspectiveLUT; }

		static constexpr GLuint kTransmittanceUnit = 20;
		static constexpr GLuint kMultiScatteringUnit = 21;
		static constexpr GLuint kSkyViewUnit = 22;
		static constexpr GLuint kAerialPerspectiveUnit = 23;

		void BindTextures();
		void BindToShader(::ShaderBase& shader);

		// Parameters
		void SetRayleighScale(float s) {
			if (s != _rayleighScale) {
				_rayleighScale = s;
				_needsPrecompute = true;
			}
		}

		float GetRayleighScale() const { return _rayleighScale; }

		void SetMieScale(float s) {
			if (s != _mieScale) {
				_mieScale = s;
				_needsPrecompute = true;
			}
		}

		float GetMieScale() const { return _mieScale; }

		void SetMieAnisotropy(const glm::vec3& g) {
			if (g != _mieAnisotropy) {
				_mieAnisotropy = g;
				_needsPrecompute = true;
			}
		}

		glm::vec3 GetMieAnisotropy() const { return _mieAnisotropy; }

		void SetMultiScatteringScale(float s) { _multiScatScale = s; }

		float GetMultiScatteringScale() const { return _multiScatScale; }

		void SetAmbientScatteringScale(float s) { _ambientScatScale = s; }

		float GetAmbientScatteringScale() const { return _ambientScatScale; }

		void SetAtmosphereHeight(float h) {
			if (h != _atmosphereHeight) {
				_atmosphereHeight = h;
				_needsPrecompute = true;
			}
		}

		float GetAtmosphereHeight() const { return _atmosphereHeight; }

		void SetRayleighScattering(const glm::vec3& s) {
			if (s != _rayleighScattering) {
				_rayleighScattering = s;
				_needsPrecompute = true;
			}
		}

		glm::vec3 GetRayleighScattering() const { return _rayleighScattering; }

		void SetMieScattering(const glm::vec3& s) {
			if (s != _mieScattering) {
				_mieScattering = s;
				_needsPrecompute = true;
			}
		}

		glm::vec3 GetMieScattering() const { return _mieScattering; }

		void SetMieExtinction(const glm::vec3& e) {
			if (e != _mieExtinction) {
				_mieExtinction = e;
				_needsPrecompute = true;
			}
		}

		glm::vec3 GetMieExtinction() const { return _mieExtinction; }

		// Cloud & Atmosphere centralized getters/setters
		void SetCloudAltitude(float h) { _cloudAltitude = h; }
		float GetCloudAltitude() const { return _cloudAltitude; }

		void SetCloudThickness(float t) { _cloudThickness = t; }
		float GetCloudThickness() const { return _cloudThickness; }

		void SetCloudDensity(float d) { _cloudDensity = d; }
		float GetCloudDensity() const { return _cloudDensity; }

		void SetCloudWarp(float w) { _cloudWarp = w; }
		float GetCloudWarp() const { return _cloudWarp; }

		void SetCloudPhaseG1(const glm::vec3& g) { _cloudPhaseG1 = g; }
		glm::vec3 GetCloudPhaseG1() const { return _cloudPhaseG1; }

		void SetCloudPhaseG2(const glm::vec3& g) { _cloudPhaseG2 = g; }
		glm::vec3 GetCloudPhaseG2() const { return _cloudPhaseG2; }

		void SetCloudPhaseAlpha(float a) { _cloudPhaseAlpha = a; }
		float GetCloudPhaseAlpha() const { return _cloudPhaseAlpha; }

		void SetCloudPhaseIsotropic(float i) { _cloudPhaseIsotropic = i; }
		float GetCloudPhaseIsotropic() const { return _cloudPhaseIsotropic; }

		void SetCloudPowderScale(float s) { _cloudPowderScale = s; }
		float GetCloudPowderScale() const { return _cloudPowderScale; }

		void SetCloudPowderMultiplier(float m) { _cloudPowderMultiplier = m; }
		float GetCloudPowderMultiplier() const { return _cloudPowderMultiplier; }

		void SetCloudPowderLocalScale(float s) { _cloudPowderLocalScale = s; }
		float GetCloudPowderLocalScale() const { return _cloudPowderLocalScale; }

		void SetCloudShadowOpticalDepthMultiplier(float m) { _cloudShadowOpticalDepthMultiplier = m; }
		float GetCloudShadowOpticalDepthMultiplier() const { return _cloudShadowOpticalDepthMultiplier; }

		void SetCloudShadowStepMultiplier(float m) { _cloudShadowStepMultiplier = m; }
		float GetCloudShadowStepMultiplier() const { return _cloudShadowStepMultiplier; }

		void SetCloudSunLightScale(float s) { _cloudSunLightScale = s; }
		float GetCloudSunLightScale() const { return _cloudSunLightScale; }

		void SetCloudMoonLightScale(float s) { _cloudMoonLightScale = s; }
		float GetCloudMoonLightScale() const { return _cloudMoonLightScale; }

		void SetCloudBeerPowderMix(float m) { _cloudBeerPowderMix = m; }
		float GetCloudBeerPowderMix() const { return _cloudBeerPowderMix; }

		void SetCloudFlowSpeed(float s) { _cloudFlowSpeed = s; }
		float GetCloudFlowSpeed() const { return _cloudFlowSpeed; }

		void SetCloudFlowDirection(float d) { _cloudFlowDirection = d; }
		float GetCloudFlowDirection() const { return _cloudFlowDirection; }

		void SetCloudFlowHeightScale(float s) { _cloudFlowHeightScale = s; }
		float GetCloudFlowHeightScale() const { return _cloudFlowHeightScale; }

		void SetCloudCurlStrength(float s) { _cloudCurlStrength = s; }
		float GetCloudCurlStrength() const { return _cloudCurlStrength; }

		void SetCloudCurlFrequency(float f) { _cloudCurlFrequency = f; }
		float GetCloudCurlFrequency() const { return _cloudCurlFrequency; }

		void SetOzoneAbsorption(const glm::vec3& a) {
			if (a != _ozoneAbsorption) {
				_ozoneAbsorption = a;
				_needsPrecompute = true;
			}
		}

		glm::vec3 GetOzoneAbsorption() const { return _ozoneAbsorption; }

		void SetRayleighScaleHeight(float h) {
			if (h != _rayleighScaleHeight) {
				_rayleighScaleHeight = h;
				_needsPrecompute = true;
			}
		}

		float GetRayleighScaleHeight() const { return _rayleighScaleHeight; }

		void SetMieScaleHeight(float h) {
			if (h != _mieScaleHeight) {
				_mieScaleHeight = h;
				_needsPrecompute = true;
			}
		}

		float GetMieScaleHeight() const { return _mieScaleHeight; }

		void SetColorVarianceScale(float s) { _colorVarianceScale = s; }

		float GetColorVarianceScale() const { return _colorVarianceScale; }

		void SetColorVarianceStrength(float s) { _colorVarianceStrength = s; }

		float GetColorVarianceStrength() const { return _colorVarianceStrength; }

		void SetCloudShadowIntensity(float i) { _cloudShadowIntensity = i; }

		float GetCloudShadowIntensity() const { return _cloudShadowIntensity; }

		void SetCloudCoverage(float c) {
			if (std::abs(_cloudCoverage - c) > 0.005f) {
				_cloudCoverage = c;
				_needsWeatherBake = true;
			}
		}
		float GetCloudCoverage() const { return _cloudCoverage; }

		void SetWorldScale(float s) {
			if (std::abs(_worldScale - s) > 0.001f) {
				_worldScale = s;
				_needsWeatherBake = true;
			}
		}
		float GetWorldScale() const { return _worldScale; }

		GLuint GetCloudWeatherTexture() const { return _cloudWeatherTexture; }

		/**
		 * Sample the cloud weather data on the CPU.
		 * @param worldXZ Position in world space
		 * @param time Current simulation time
		 * @return RGBA weather data (x: SDF, y: altitudeMap, z: cellID, w: thicknessMap)
		 */
		glm::vec4 GetCloudWeather(glm::vec2 worldXZ, float time) const;

		const std::vector<glm::vec4>& GetCloudSeeds() const { return _cpuCloudSeeds; }

		void SetSunAureoleStrength(float s) { _sunAureoleStrength = s; }

		float GetSunAureoleStrength() const { return _sunAureoleStrength; }

		void SetCirrusOpacity(float o) { _cirrusOpacity = o; }

		float GetCirrusOpacity() const { return _cirrusOpacity; }

		const glm::vec4* GetSHCoefficients() const { return _shCoeffs; }

		// Copy SH coefficients directly from GPU SSBO into a UBO, avoiding CPU readback
		void CopySHToUBO(GLuint lightingUbo, size_t shOffset);

	private:
		void CreateTextures();
		void CreateShaders();

		GLuint _transmittanceLUT = 0;
		GLuint _multiScatteringLUT = 0;
		GLuint _skyViewLUT = 0;
		GLuint _aerialPerspectiveLUT = 0;
		GLuint _cloudWeatherTexture = 0;
		GLuint _cloudVolumeTexture = 0;
		GLuint _cloudSeedsBuffer = 0;
		GLuint _shCoeffsBuffer = 0;

		std::unique_ptr<ComputeShader> _transmittanceShader;
		std::unique_ptr<ComputeShader> _multiScatteringShader;
		std::unique_ptr<ComputeShader> _skyViewShader;
		std::unique_ptr<ComputeShader> _aerialPerspectiveShader;
		std::unique_ptr<ComputeShader> _skyToSHShader;
		std::unique_ptr<ComputeShader> _cloudBakeShader;
		std::unique_ptr<ComputeShader> _cloudVolumeBakeShader;

		glm::vec4 _shCoeffs[81];

		bool _needsPrecompute = true;
		bool _needsWeatherBake = true;

		std::vector<glm::vec4> _cpuWeatherMap;
		std::vector<glm::vec4> _cpuCloudSeeds;
		float _cloudCoverage = WeatherConstants::CloudCoverage.normal;
		float _worldScale = 1.0f;

		float     _rayleighScale = WeatherConstants::RayleighScale.normal;
		float     _mieScale = WeatherConstants::MieScale.normal;
		glm::vec3 _mieAnisotropy = glm::vec3(WeatherConstants::MieAnisotropy);
		float     _multiScatScale = 1.0f;
		float     _ambientScatScale = 1.0f;
		float     _atmosphereHeight = WeatherConstants::AtmosphereHeight.normal;
		glm::vec3 _rayleighScattering = WeatherConstants::RayleighScattering;
		glm::vec3 _mieScattering = glm::vec3(WeatherConstants::MieScattering);
		glm::vec3 _mieExtinction = glm::vec3(WeatherConstants::MieExtinction);
		glm::vec3 _ozoneAbsorption = WeatherConstants::OzoneAbsorption;
		float     _rayleighScaleHeight = WeatherConstants::RayleighScaleHeight.normal;
		float     _mieScaleHeight = WeatherConstants::MieScaleHeight.normal;
		float     _colorVarianceScale = 1.0f;
		float     _colorVarianceStrength = 0.0f;
		float     _cloudShadowIntensity = 0.5f;
		float     _sunAureoleStrength = 0.5f;
		float     _cirrusOpacity = 0.3f;

		// Cloud & Atmosphere centralized parameters (removed from LightingUbo)
		float     _cloudAltitude = 1500.0f;
		float     _cloudThickness = 400.0f;
		float     _cloudDensity = 0.5f;
		float     _cloudWarp = 75.0f;
		glm::vec3 _cloudPhaseG1 = glm::vec3(0.875f);
		glm::vec3 _cloudPhaseG2 = glm::vec3(-0.3f);
		float     _cloudPhaseAlpha = 0.181f;
		float     _cloudPhaseIsotropic = 0.426f;
		float     _cloudPowderScale = 0.125f;
		float     _cloudPowderMultiplier = 5.0f;
		float     _cloudPowderLocalScale = 5.0f;
		float     _cloudShadowOpticalDepthMultiplier = 0.05f;
		float     _cloudShadowStepMultiplier = 0.15f;
		float     _cloudSunLightScale = 25.0f;
		float     _cloudMoonLightScale = 2.0f;
		float     _cloudBeerPowderMix = 0.6f;
		float     _cloudFlowSpeed = 0.25f;
		float     _cloudFlowDirection = glm::radians(180.0f);
		float     _cloudFlowHeightScale = 0.015f;
		float     _cloudCurlStrength = 10.0f;
		float     _cloudCurlFrequency = 2.0f;

		glm::vec3 _ambientEstimate = glm::vec3(0.0f);
	};

} // namespace Boidsish

#endif // ATMOSPHERE_MANAGER_H
