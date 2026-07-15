#include "atmosphere_manager.h"

#include <iostream>

#include "weather_manager.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include "profiler.h"
#include "service_locator.h"
#include <cstring>
#include "shader.h"

namespace Boidsish {
	AtmosphereManager::AtmosphereManager(ServiceLocator& /*loc*/) {}

	AtmosphereManager::~AtmosphereManager() {
		if (_transmittanceLUT)
			glDeleteTextures(1, &_transmittanceLUT);
		if (_multiScatteringLUT)
			glDeleteTextures(1, &_multiScatteringLUT);
		if (_skyViewLUT)
			glDeleteTextures(1, &_skyViewLUT);
		if (_aerialPerspectiveLUT)
			glDeleteTextures(1, &_aerialPerspectiveLUT);
		if (_cloudWeatherTexture)
			glDeleteTextures(1, &_cloudWeatherTexture);
		if (_cloudSdf3DTexture)
			glDeleteTextures(1, &_cloudSdf3DTexture);
		if (_cloudSeedsBuffer)
			glDeleteBuffers(1, &_cloudSeedsBuffer);
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

		// Cloud Weather Map: 2048x2048 RGBA16F
		glGenTextures(1, &_cloudWeatherTexture);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 2048, 2048, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Cloud 3D SDF Texture: 128x32x128 RGBA16F (Volume)
		glGenTextures(1, &_cloudSdf3DTexture);
		glBindTexture(GL_TEXTURE_3D, _cloudSdf3DTexture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 128, 32, 128, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		// SH Coefficients SSBO: 9 x vec4
		// Cloud Seeds SSBO: 100 x vec4 (10x10 Voronoi period)
		glGenBuffers(1, &_cloudSeedsBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _cloudSeedsBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 100 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// SH Coefficients SSBO: 81 x vec4 (9 probes * 9 coeffs)
		glGenBuffers(1, &_shCoeffsBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _shCoeffsBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 81 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		for (int i = 0; i < 81; ++i) {
			_shCoeffs[i] = glm::vec4(0.0f);
		}

		auto& reg = GpuResourceRegistry::Instance();
		reg.PublishTexture(Constants::TextureUnit::AtmosphereTransmittance(), _transmittanceLUT);
		reg.PublishTexture(Constants::TextureUnit::AtmosphereMultiScattering(), _multiScatteringLUT);
		reg.PublishTexture(Constants::TextureUnit::AtmosphereSkyView(), _skyViewLUT);
		reg.PublishTexture(Constants::TextureUnit::AtmosphereAerialPerspective(), _aerialPerspectiveLUT, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::CloudWeatherBake(), _cloudWeatherTexture);
		reg.PublishTexture(Constants::TextureUnit::CloudSdf3D(), _cloudSdf3DTexture, GL_TEXTURE_3D);
	}

	void AtmosphereManager::CreateShaders() {
		auto setup_shader = [](ComputeShader& s) {
			s.use();
			s.bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
			s.bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
		};

		_transmittanceShader = std::make_unique<ComputeShader>("shaders/atmosphere/transmittance_lut.comp");
		_multiScatteringShader = std::make_unique<ComputeShader>("shaders/atmosphere/multiscattering_lut.comp");
		_skyViewShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_view_lut.comp");
		_aerialPerspectiveShader = std::make_unique<ComputeShader>("shaders/atmosphere/aerial_perspective_lut.comp");
		_skyToSHShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_to_sh.comp");
		_cloudBakeShader = std::make_unique<ComputeShader>("shaders/effects/cloud_weather_bake.comp");
		_cloudSdf3DBakeShader = std::make_unique<ComputeShader>("shaders/effects/cloud_3d_sdf_bake.comp");

		setup_shader(*_transmittanceShader);
		setup_shader(*_multiScatteringShader);
		setup_shader(*_skyViewShader);
		setup_shader(*_aerialPerspectiveShader);
		setup_shader(*_skyToSHShader);
		setup_shader(*_cloudBakeShader);
		setup_shader(*_cloudSdf3DBakeShader);
	}

	void AtmosphereManager::Update(
		const glm::vec3& sunDir,
		const glm::vec3& sunColor,
		float            sunIntensity,
		const glm::vec3& cameraPos,
		float            time,
		float            worldScale
	) {
		PROJECT_PROFILE_SCOPE("AtmosphereManager::Update");
		if (_needsWeatherBake) {
			// Clear seeds buffer before bake
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _cloudSeedsBuffer);
			std::vector<glm::vec4> clearData(100, glm::vec4(0, 0, 100000.0f, 0));
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 100 * sizeof(glm::vec4), clearData.data());

			_cloudBakeShader->use();
			_cloudBakeShader->setFloat("uCloudCoverage", _cloudCoverage);
			_cloudBakeShader->setFloat("uWorldScale", worldScale);
			glBindImageTexture(0, _cloudWeatherTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudSeeds(), _cloudSeedsBuffer);
			GpuResourceRegistry::Instance().BindTextures({Constants::TextureUnit::NoiseExtra()});
			glDispatchCompute(2048 / 16, 2048 / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// CPU Readback for weather queries
			_cpuWeatherMap.resize(2048 * 2048);
			glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, _cpuWeatherMap.data());

			// CPU Readback for seeds
			_cpuCloudSeeds.resize(100);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _cloudSeedsBuffer);
			void* ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			if (ptr) {
				memcpy(_cpuCloudSeeds.data(), ptr, 100 * sizeof(glm::vec4));
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			}

			// Dispatch 3D SDF Baking (every frame to ensure perfect sync with dynamic cloud altitude/thickness)
			_cloudSdf3DBakeShader->use();
			glBindImageTexture(0, _cloudSdf3DTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute(128 / 8, 32 / 4, 128 / 8); // workgroup size: 8x4x8
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


			_needsWeatherBake = false;
			_worldScale = worldScale;
		}

		if (_needsPrecompute) {
			// Dispatch Transmittance
			_transmittanceShader->use();
			_transmittanceShader->setFloat("u_rayleighScale", _rayleighScale);
			_transmittanceShader->setFloat("u_mieScale", _mieScale);

			_transmittanceShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
			_transmittanceShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
			_transmittanceShader->setVec3("u_mieScatteringBase", _mieScattering);
			_transmittanceShader->setVec3("u_mieExtinctionBase", _mieExtinction);
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
			_multiScatteringShader->setVec3("u_mieAnisotropy", _mieAnisotropy);

			_multiScatteringShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
			_multiScatteringShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
			_multiScatteringShader->setVec3("u_mieScatteringBase", _mieScattering);
			_multiScatteringShader->setVec3("u_mieExtinctionBase", _mieExtinction);
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
		_skyViewShader->setVec3("u_mieAnisotropy", _mieAnisotropy);
		_skyViewShader->setFloat("u_multiScatScale", _multiScatScale);

		_skyViewShader->setFloat("u_worldScale", worldScale);
		_skyViewShader->setFloat("u_cloudShadowIntensity", _cloudShadowIntensity);

		_skyViewShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
		_skyViewShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
		_skyViewShader->setVec3("u_mieScatteringBase", _mieScattering);
		_skyViewShader->setVec3("u_mieExtinctionBase", _mieExtinction);
		_skyViewShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
		_skyViewShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		_skyViewShader->setFloat("u_mieScaleHeight", _mieScaleHeight);
		_skyViewShader->setFloat("u_colorVarianceScale", _colorVarianceScale);
		_skyViewShader->setFloat("u_colorVarianceStrength", _colorVarianceStrength);
		_skyViewShader->setFloat("u_sunAureoleStrength", _sunAureoleStrength);
		_skyViewShader->setFloat("u_cirrusOpacity", _cirrusOpacity);

		auto wm = ServiceLocator::Instance().Get<WeatherManager>();
		auto weather = wm->GetCurrentWeather();
		_skyViewShader->setFloat("hazeDensity", weather.haze_density);
		_skyViewShader->setFloat("hazeHeight", weather.haze_height);

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
		_aerialPerspectiveShader->setVec3("u_mieAnisotropy", _mieAnisotropy);
		_aerialPerspectiveShader->setFloat("u_multiScatScale", _multiScatScale);
		_aerialPerspectiveShader->setFloat("u_ambientScatScale", _ambientScatScale);

		_aerialPerspectiveShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
		_aerialPerspectiveShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
		_aerialPerspectiveShader->setVec3("u_mieScatteringBase", _mieScattering);
		_aerialPerspectiveShader->setVec3("u_mieExtinctionBase", _mieExtinction);
		_aerialPerspectiveShader->setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
		_aerialPerspectiveShader->setFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		_aerialPerspectiveShader->setFloat("u_mieScaleHeight", _mieScaleHeight);
		_aerialPerspectiveShader->setFloat("u_colorVarianceScale", _colorVarianceScale);
		_aerialPerspectiveShader->setFloat("u_colorVarianceStrength", _colorVarianceStrength);
		_aerialPerspectiveShader->setFloat("u_sunAureoleStrength", _sunAureoleStrength);
		_aerialPerspectiveShader->setFloat("u_cirrusOpacity", _cirrusOpacity);


		_aerialPerspectiveShader->setFloat("hazeDensity", weather.haze_density);
		_aerialPerspectiveShader->setFloat("hazeHeight", weather.haze_height);
		_aerialPerspectiveShader->setVec3("hazeColor", weather.haze_color);


		glDispatchCompute(32 / 4, 32 / 4, 32 / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Dispatch SkyToSH
		_skyToSHShader->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _skyViewLUT);
		// glActiveTexture(GL_TEXTURE1);
		// glBindTexture(GL_TEXTURE_3D, _aerialPerspectiveLUT);
		_skyToSHShader->setInt("u_skyViewLUT", 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AtmosphereSH(), _shCoeffsBuffer);
		glDispatchCompute(1, 1, 9); // Dispatch 9 workgroups (Z dimension), one for each spatial probe
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

	void AtmosphereManager::CopySHToUBO(GLuint lightingUbo, size_t shOffset) {
		if (_shCoeffsBuffer == 0)
			return;
		glBindBuffer(GL_COPY_READ_BUFFER, _shCoeffsBuffer);
		glBindBuffer(GL_COPY_WRITE_BUFFER, lightingUbo);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, shOffset, 81 * sizeof(glm::vec4));
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	void AtmosphereManager::BindTextures() {
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
		glBindTexture(GL_TEXTURE_2D, _transmittanceLUT);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereMultiScattering());
		glBindTexture(GL_TEXTURE_2D, _multiScatteringLUT);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereSkyView());
		glBindTexture(GL_TEXTURE_2D, _skyViewLUT);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
		glBindTexture(GL_TEXTURE_3D, _aerialPerspectiveLUT);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherBake());
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudSdf3D());
		glBindTexture(GL_TEXTURE_3D, _cloudSdf3DTexture);
	}

	void AtmosphereManager::BindToShader(::ShaderBase& shader) {
		BindTextures();
		shader.trySetInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());
		shader.trySetInt("u_multiScatteringLUT", Constants::TextureUnit::AtmosphereMultiScattering());
		shader.trySetInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());
		shader.trySetInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());
		shader.trySetInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
		shader.trySetInt("u_cloudSdf3DTexture", Constants::TextureUnit::CloudSdf3D());
		shader.trySetFloat("u_atmosphereHeight", _atmosphereHeight);

		shader.setVec3("u_rayleighScatteringBase", _rayleighScattering);
		shader.trySetFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		shader.setVec3("u_mieScatteringBase", _mieScattering);
		shader.setVec3("u_mieExtinctionBase", _mieExtinction);
		shader.trySetFloat("u_mieScaleHeight", _mieScaleHeight);
		shader.trySetFloat("u_rayleighScale", _rayleighScale);
		shader.trySetFloat("u_mieScale", _mieScale);
		shader.setVec3("u_mieAnisotropy", _mieAnisotropy);
		shader.setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
	}

	glm::vec4 AtmosphereManager::GetCloudWeather(glm::vec2 worldXZ, float time) const {
		if (_cpuWeatherMap.empty()) return glm::vec4(100000.0f, 0, 0, 0);

		// factor in advection (matches shaders/helpers/clouds.glsl)
		float     angle = 3.14f; // cloudFlow constant
		glm::vec2 flowDir = glm::vec2(cos(angle), sin(angle));
		glm::vec2 advect = flowDir * 5.0f * _worldScale * 10.0f * time;

		glm::vec2 p_advected = worldXZ + advect;
		float     mapRange = 100000.0f * _worldScale;

		float u = p_advected.x / mapRange;
		float v = p_advected.y / mapRange;

		// Toroidal wrapping
		u = fmod(u, 1.0f); if (u < 0.0f) u += 1.0f;
		v = fmod(v, 1.0f); if (v < 0.0f) v += 1.0f;

		int x = (int)(u * 2047.0f);
		int y = (int)(v * 2047.0f);

		return _cpuWeatherMap[y * 2048 + x];
	}

} // namespace Boidsish
