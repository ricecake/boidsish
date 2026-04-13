#include "atmosphere_manager.h"

#include <iostream>

#include "profiler.h"
#include "shader.h"

namespace Boidsish {

	AtmosphereManager::AtmosphereManager() {}

	AtmosphereManager::~AtmosphereManager() {
		if (_transmittanceLUT)
			glDeleteTextures(1, &_transmittanceLUT);
		if (_multiScatteringLUT)
			glDeleteTextures(1, &_multiScatteringLUT);
		if (_skyViewLUT)
			glDeleteTextures(1, &_skyViewLUT);
		if (_aerialPerspectiveLUT)
			glDeleteTextures(1, &_aerialPerspectiveLUT);
		if (_shCoeffsBuffer)
			glDeleteBuffers(1, &_shCoeffsBuffer);
	}

	void AtmosphereManager::Initialize() {
		CreateTextures();
		CreateShaders();
	}

	void AtmosphereManager::CreateTextures() {
		// Transmittance LUT: 256x64 RGBA32F
		glGenTextures(1, &_transmittanceLUT);
		glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 64, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// MultiScattering LUT: 32x32 RGBA32F
		glGenTextures(1, &_multiScatteringLUT);
		glBindTexture(GL_TEXTURE_2D, _multiScatteringLUT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 32, 32, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// SkyView LUT: 192x108 RGBA32F
		glGenTextures(1, &_skyViewLUT);
		glBindTexture(GL_TEXTURE_2D, _skyViewLUT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 192, 108, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// AerialPerspective LUT: 32x32x32 RGBA32F (Volume)
		glGenTextures(1, &_aerialPerspectiveLUT);
		glBindTexture(GL_TEXTURE_3D, _aerialPerspectiveLUT);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, 32, 32, 32, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// SH Coefficients SSBO: 9 x vec4
		glGenBuffers(1, &_shCoeffsBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _shCoeffsBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 9 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		for (int i = 0; i < 9; ++i) {
			_shCoeffs[i] = glm::vec4(0.0f);
		}
	}

	void AtmosphereManager::CreateShaders() {
		_transmittanceShader = std::make_unique<ComputeShader>("shaders/atmosphere/transmittance_lut.comp");
		_multiScatteringShader = std::make_unique<ComputeShader>("shaders/atmosphere/multiscattering_lut.comp");
		_skyViewShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_view_lut.comp");
		_aerialPerspectiveShader = std::make_unique<ComputeShader>("shaders/atmosphere/aerial_perspective_lut.comp");
		_skyToSHShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_to_sh.comp");
	}

	void AtmosphereManager::Update(
		const glm::vec3& sunDir,
		const glm::vec3& sunColor,
		float            sunIntensity,
		const glm::vec3& cameraPos,
		float            time
	) {
		PROJECT_PROFILE_SCOPE("AtmosphereManager::Update");
		if (_needsPrecompute) {
			// Dispatch Transmittance
			_transmittanceShader->use();
			_transmittanceShader->setFloat("u_rayleighScale", _rayleighScale);
			_transmittanceShader->setFloat("u_mieScale", _mieScale);

			_transmittanceShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
			_transmittanceShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
			_transmittanceShader->setFloat("u_mieScatteringBase", _mieScattering);
			_transmittanceShader->setFloat("u_mieExtinctionBase", _mieExtinction);
			_transmittanceShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
			_transmittanceShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
			_transmittanceShader->setFloat("u_mieScaleHeight", _mieScaleHeight);

			glBindImageTexture(0, _transmittanceLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glDispatchCompute(256 / 8, 64 / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Dispatch MultiScattering
			_multiScatteringShader->use();
			_multiScatteringShader->setFloat("u_rayleighScale", _rayleighScale);
			_multiScatteringShader->setFloat("u_mieScale", _mieScale);
			_multiScatteringShader->setFloat("u_mieAnisotropy", _mieAnisotropy);

			_multiScatteringShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
			_multiScatteringShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
			_multiScatteringShader->setFloat("u_mieScatteringBase", _mieScattering);
			_multiScatteringShader->setFloat("u_mieExtinctionBase", _mieExtinction);
			_multiScatteringShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
			_multiScatteringShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
			_multiScatteringShader->setFloat("u_mieScaleHeight", _mieScaleHeight);

			glBindImageTexture(0, _multiScatteringLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
			_multiScatteringShader->setInt("u_transmittanceLUT", 1);
			glDispatchCompute(1, 1, 1); // Local size is 32x32, which matches texture size
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			_needsPrecompute = false;
		}

		// Dispatch SkyView
		_skyViewShader->use();
		glBindImageTexture(0, _skyViewLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, _multiScatteringLUT);
		_skyViewShader->setInt("u_transmittanceLUT", 1);
		_skyViewShader->setInt("u_multiScatteringLUT", 2);
		_skyViewShader->setVec3("u_sunDir", sunDir);
		_skyViewShader->setVec3("u_sunRadiance", sunColor * sunIntensity);
		_skyViewShader->setVec3("u_cameraPos", cameraPos);
		_skyViewShader->setFloat("u_time", time);
		_skyViewShader->setFloat("u_rayleighScale", _rayleighScale);
		_skyViewShader->setFloat("u_mieScale", _mieScale);
		_skyViewShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
		_skyViewShader->setFloat("u_multiScatScale", _multiScatScale);

		_skyViewShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
		_skyViewShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
		_skyViewShader->setFloat("u_mieScatteringBase", _mieScattering);
		_skyViewShader->setFloat("u_mieExtinctionBase", _mieExtinction);
		_skyViewShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
		_skyViewShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		_skyViewShader->setFloat("u_mieScaleHeight", _mieScaleHeight);
		_skyViewShader->setFloat("u_colorVarianceScale", _colorVarianceScale);
		_skyViewShader->setFloat("u_colorVarianceStrength", _colorVarianceStrength);

		glDispatchCompute(192 / 8, (108 + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Dispatch AerialPerspective
		_aerialPerspectiveShader->use();
		glBindImageTexture(0, _aerialPerspectiveLUT, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, _multiScatteringLUT);
		_aerialPerspectiveShader->setInt("u_transmittanceLUT", 1);
		_aerialPerspectiveShader->setInt("u_multiScatteringLUT", 2);
		_aerialPerspectiveShader->setVec3("u_sunDir", sunDir);
		_aerialPerspectiveShader->setVec3("u_sunRadiance", sunColor * sunIntensity);
		_aerialPerspectiveShader->setVec3("u_cameraPos", cameraPos);
		_aerialPerspectiveShader->setFloat("u_time", time);
		_aerialPerspectiveShader->setFloat("u_rayleighScale", _rayleighScale);
		_aerialPerspectiveShader->setFloat("u_mieScale", _mieScale);
		_aerialPerspectiveShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
		_aerialPerspectiveShader->setFloat("u_multiScatScale", _multiScatScale);
		_aerialPerspectiveShader->setFloat("u_ambientScatScale", _ambientScatScale);

		_aerialPerspectiveShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
		_aerialPerspectiveShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
		_aerialPerspectiveShader->setFloat("u_mieScatteringBase", _mieScattering);
		_aerialPerspectiveShader->setFloat("u_mieExtinctionBase", _mieExtinction);
		_aerialPerspectiveShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
		_aerialPerspectiveShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		_aerialPerspectiveShader->setFloat("u_mieScaleHeight", _mieScaleHeight);
		_aerialPerspectiveShader->setFloat("u_colorVarianceScale", _colorVarianceScale);
		_aerialPerspectiveShader->setFloat("u_colorVarianceStrength", _colorVarianceStrength);

		glDispatchCompute(32 / 4, 32 / 4, 32 / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Dispatch SkyToSH
		_skyToSHShader->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _skyViewLUT);
		_skyToSHShader->setInt("u_skyViewLUT", 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _shCoeffsBuffer);
		glDispatchCompute(1, 1, 1); // Logic in sky_to_sh.comp uses a single workgroup for simple integration
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// SH coefficients remain on GPU — copied to UBO via CopySHToUBO() later.
		// No CPU readback needed.

		// Analytical estimate of sky ambient irradiance for synchronization with other systems
		float sunElevation = sunDir.y;

		// Rayleigh and Mie contribute differently to global irradiance
		// These constants are tuned to match the visual output of the LUTs
		float rayleighIrradiance = _rayleighScale * 0.05f;
		float mieIrradiance = _mieScale * 0.02f;

		// Global factor based on sun elevation.
		// Atmosphere stays lit even slightly after sunset (civil twilight)
		// We use a smoothstep to avoid the "flipping a switch" feeling
		float horizonFactor = glm::smoothstep(-0.2f, 0.5f, sunElevation);

		// Multi-scattering and Rayleigh are the primary drivers of ambient sky light
		float ambientFactor = horizonFactor * (rayleighIrradiance + mieIrradiance) * _multiScatScale;

		// Night base ambient to ensure world isn't pitch black
		glm::vec3 nightGlow = glm::vec3(0.01f, 0.012f, 0.018f) * _ambientScatScale * 10.0f;

		_ambientEstimate = sunColor * sunIntensity * ambientFactor + nightGlow;
	}

	void AtmosphereManager::CopySHToUBO(GLuint lightingUbo, size_t shOffset, size_t uboTotalOffset) {
		if (_shCoeffsBuffer == 0)
			return;
		glBindBuffer(GL_COPY_READ_BUFFER, _shCoeffsBuffer);
		glBindBuffer(GL_COPY_WRITE_BUFFER, lightingUbo);
		glCopyBufferSubData(
			GL_COPY_READ_BUFFER,
			GL_COPY_WRITE_BUFFER,
			0,
			uboTotalOffset + shOffset,
			9 * sizeof(glm::vec4)
		);
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	void AtmosphereManager::BindTextures(GLuint firstUnit) {
		glActiveTexture(GL_TEXTURE0 + firstUnit);
		glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
		glActiveTexture(GL_TEXTURE0 + firstUnit + 1);
		glBindTexture(GL_TEXTURE_2D, _multiScatteringLUT);
		glActiveTexture(GL_TEXTURE0 + firstUnit + 2);
		glBindTexture(GL_TEXTURE_2D, _skyViewLUT);
		glActiveTexture(GL_TEXTURE0 + firstUnit + 3);
		glBindTexture(GL_TEXTURE_3D, _aerialPerspectiveLUT);
	}

} // namespace Boidsish
