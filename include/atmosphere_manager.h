#ifndef ATMOSPHERE_MANAGER_H
#define ATMOSPHERE_MANAGER_H

#include <memory>
#include <vector>
#include <string>

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
			float            worldScale,
			const glm::vec3& moonDir = glm::vec3(0.0f, -1.0f, 0.0f),
			const glm::vec3& moonRadiance = glm::vec3(0.0f)
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

		void SetMieAnisotropy(float g) {
			if (g != _mieAnisotropy) {
				_mieAnisotropy = g;
				_needsPrecompute = true;
			}
		}

		float GetMieAnisotropy() const { return _mieAnisotropy; }

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

		void SetMieScattering(float s) {
			if (s != _mieScattering) {
				_mieScattering = s;
				_needsPrecompute = true;
			}
		}

		float GetMieScattering() const { return _mieScattering; }

		void SetMieExtinction(float e) {
			if (e != _mieExtinction) {
				_mieExtinction = e;
				_needsPrecompute = true;
			}
		}

		float GetMieExtinction() const { return _mieExtinction; }

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
			}
		}
		float GetCloudCoverage() const { return _cloudCoverage; }

		void SetCloudAltitude(float h) {
			if (h != _cloudAltitude) {
				_cloudAltitude = h;
			}
		}
		float GetCloudAltitude() const { return _cloudAltitude; }

		void SetCloudThickness(float t) {
			if (t != _cloudThickness) {
				_cloudThickness = t;
			}
		}
		float GetCloudThickness() const { return _cloudThickness; }

		void SetCloudDensity(float d) {
			if (d != _cloudDensity) {
				_cloudDensity = d;
			}
		}
		float GetCloudDensity() const { return _cloudDensity; }

		void SetCloudFlowSpeed(float s) { _cloudFlowSpeed = s; }
		float GetCloudFlowSpeed() const { return _cloudFlowSpeed; }

		void SetCloudFlowDirection(float d) { _cloudFlowDirection = d; }
		float GetCloudFlowDirection() const { return _cloudFlowDirection; }

		void SetWorldScale(float s) {
			if (std::abs(_worldScale - s) > 0.001f) {
				_worldScale = s;
				_needsWeatherBake = true;
			}
		}
		float GetWorldScale() const { return _worldScale; }

		GLuint GetCloudWeatherTexture() const { return _cloudWeatherTexture; }
		GLuint GetCloudWeatherMinMaxTexture() const { return _cloudWeatherMinMaxTexture; }
		GLuint GetCloudShadowTexture() const { return _cloudShadowTexture; }
		GLuint GetCloud2DPropsLUT() const { return _cloud2DPropsLUT; }
		GLuint GetCloud3DFrontLUT() const { return _cloud3DFrontLUT; }
		const glm::mat4& GetCloudShadowMatrix() const { return _cloudShadowMatrix; }
		const glm::mat4& GetCloudShadowInvMatrix() const { return _cloudShadowInvMatrix; }

		bool IsCustomWeatherMap() const { return _useCustomWeatherMap; }
		void SetUseCustomWeatherMap(bool b);

		bool ExportCloudWeatherMap(const std::string& filepath);
		bool ImportCloudWeatherMap(const std::string& filepath);

		void PaintWeatherMap(
			const glm::vec2& brushCenterUV,
			float            brushRadiusUV,
			const glm::vec4& brushValue,
			const glm::vec4& channelMask,
			int              drawOp,
			float            brushStrength,
			float            smoothness
		);

		void RebakeWeatherMinMaxAndVolume();
		void ForceRebakeWeatherMap();
		void ClearWeatherMap();

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
		GLuint _cloudWeatherMinMaxTexture = 0;
		GLuint _cloudVolumeTexture = 0;
		GLuint _cloudShadowTexture = 0;
		GLuint _cloud2DPropsLUT = 0;
		GLuint _cloud3DFrontLUT = 0;
		GLuint _cloudSeedsBuffer = 0;
		GLuint _shCoeffsBuffer = 0;

		std::unique_ptr<ComputeShader> _transmittanceShader;
		std::unique_ptr<ComputeShader> _multiScatteringShader;
		std::unique_ptr<ComputeShader> _skyViewShader;
		std::unique_ptr<ComputeShader> _aerialPerspectiveShader;
		std::unique_ptr<ComputeShader> _skyToSHShader;
		std::unique_ptr<ComputeShader> _cloudBakeShader;
		std::unique_ptr<ComputeShader> _cloudVolumeBakeShader;
		std::unique_ptr<ComputeShader> _cloudMipShader;
		std::unique_ptr<ComputeShader> _cloudShadowBakeShader;
		std::unique_ptr<ComputeShader> _cloudPaintShader;
		std::unique_ptr<ComputeShader> _cloudMinMaxInitShader;

		glm::mat4 _cloudShadowMatrix = glm::mat4(1.0f);
		glm::mat4 _cloudShadowInvMatrix = glm::mat4(1.0f);
		glm::vec3 _lastBakedLightDir = glm::vec3(0.0f);
		glm::vec3 _lastBakedCameraPos = glm::vec3(0.0f);
		float     _lastBakedCloudCoverage = 0.0f;
		float     _lastBakedCloudDensity = 0.0f;
		float     _lastBakedCloudAltitude = 0.0f;
		float     _lastBakedCloudThickness = 0.0f;
		bool      _enableCloudShadowMap = true;
		int       _frameIndex = 0;

		glm::vec4 _shCoeffs[81];

		bool _needsPrecompute = true;
		bool _needsWeatherBake = true;
		bool _useCustomWeatherMap = false;

		std::vector<glm::vec4> _cpuWeatherMap;
		std::vector<glm::vec4> _cpuCloudSeeds;
		float _cloudCoverage = WeatherConstants::CloudCoverage.normal;
		float _cloudAltitude = WeatherConstants::CloudAltitude.normal;
		float _cloudThickness = WeatherConstants::CloudThickness.normal;
		float _cloudDensity = WeatherConstants::CloudDensity.normal;
		float _cloudFlowSpeed = 0.250f;
		float _cloudFlowDirection = 3.14159265f;
		float _worldScale = 1.0f;

		float     _rayleighScale = WeatherConstants::RayleighScale.normal;
		float     _mieScale = WeatherConstants::MieScale.normal;
		float     _mieAnisotropy = WeatherConstants::MieAnisotropy;
		float     _multiScatScale = 1.0f;
		float     _ambientScatScale = 1.0f;
		float     _atmosphereHeight = WeatherConstants::AtmosphereHeight.normal;
		glm::vec3 _rayleighScattering = WeatherConstants::RayleighScattering;
		float     _mieScattering = WeatherConstants::MieScattering;
		float     _mieExtinction = WeatherConstants::MieExtinction;
		glm::vec3 _ozoneAbsorption = WeatherConstants::OzoneAbsorption;
		float     _rayleighScaleHeight = WeatherConstants::RayleighScaleHeight.normal;
		float     _mieScaleHeight = WeatherConstants::MieScaleHeight.normal;
		float     _colorVarianceScale = 1.0f;
		float     _colorVarianceStrength = 0.0f;
		float     _cloudShadowIntensity = 0.5f;
		float     _sunAureoleStrength = 0.5f;
		float     _cirrusOpacity = 0.3f;

		glm::vec3 _ambientEstimate = glm::vec3(0.0f);
	};

} // namespace Boidsish

#endif // ATMOSPHERE_MANAGER_H
