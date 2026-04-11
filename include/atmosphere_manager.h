#ifndef ATMOSPHERE_MANAGER_H
#define ATMOSPHERE_MANAGER_H

#include <memory>
#include <vector>

#include "IManager.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "weather_constants.h"

class ComputeShader;

namespace Boidsish {

	class AtmosphereManager: public IManager {
	public:
		AtmosphereManager();
		~AtmosphereManager();

		void Initialize() override;
		void Update(
			const glm::vec3& sunDir,
			const glm::vec3& sunColor,
			float            sunIntensity,
			const glm::vec3& cameraPos,
			float            time
		);

		glm::vec3 GetAmbientEstimate() const { return _ambientEstimate; }

		GLuint GetTransmittanceLUT() const { return _transmittanceLUT; }

		GLuint GetMultiScatteringLUT() const { return _multiScatteringLUT; }

		GLuint GetSkyViewLUT() const { return _skyViewLUT; }

		GLuint GetAerialPerspectiveLUT() const { return _aerialPerspectiveLUT; }

		GLuint GetCloudShadowMap() const { return _cloudShadowMap; }

		void BindTextures(GLuint firstUnit = 10);

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
		GLuint _cloudShadowMap = 0;
		GLuint _shCoeffsBuffer = 0;

		std::unique_ptr<ComputeShader> _transmittanceShader;
		std::unique_ptr<ComputeShader> _multiScatteringShader;
		std::unique_ptr<ComputeShader> _skyViewShader;
		std::unique_ptr<ComputeShader> _aerialPerspectiveShader;
		std::unique_ptr<ComputeShader> _skyToSHShader;
		std::unique_ptr<ComputeShader> _cloudShadowShader;

		glm::vec4 _shCoeffs[9];

		bool _needsPrecompute = true;

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

		glm::vec3 _ambientEstimate = glm::vec3(0.0f);
	};

} // namespace Boidsish

#endif // ATMOSPHERE_MANAGER_H
