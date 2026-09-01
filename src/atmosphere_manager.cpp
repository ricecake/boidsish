#include "atmosphere_manager.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#include "stb_image.h"
#include "stb_image_write.h"

#include "weather_manager.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include "profiler.h"
#include "service_locator.h"
#include "light_manager.h"
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
		if (_cloudWeatherMinMaxTexture)
			glDeleteTextures(1, &_cloudWeatherMinMaxTexture);
		if (_cloudVolumeTexture)
			glDeleteTextures(1, &_cloudVolumeTexture);
		if (_cloudShadowTexture)
			glDeleteTextures(1, &_cloudShadowTexture);
		if (_cloud2DPropsLUT)
			glDeleteTextures(1, &_cloud2DPropsLUT);
		if (_cloud3DFrontLUT)
			glDeleteTextures(1, &_cloud3DFrontLUT);
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

		// Cloud Weather Map: 2048x2048 RGBA16F (1 level, no mipmaps)
		glGenTextures(1, &_cloudWeatherTexture);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glTexStorage2D(GL_TEXTURE_2D, 6, GL_RGBA16F, 2048, 2048);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Cloud Weather Min-Max Density Map: 2048x2048 RG16F (12 levels, min-max mipmaps)
		glGenTextures(1, &_cloudWeatherMinMaxTexture);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
		glTexStorage2D(GL_TEXTURE_2D, 12, GL_RG16F, 2048, 2048);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Cloud Volume Texture: 128x128x128 RGBA16F (3D)
		glGenTextures(1, &_cloudVolumeTexture);
		glBindTexture(GL_TEXTURE_3D, _cloudVolumeTexture);
		glTexImage3D(GL_TEXTURE_3D, 4, GL_RGBA16F, 128, 128, 128, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexStorage3D(GL_TEXTURE_3D, 4, GL_RGBA16F, 128, 128, 128);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
		constexpr float border[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border);

		// Cloud Shadow Map: 512x512 R16F 2D Array (8 layers, with mips for soft AO)
		glGenTextures(1, &_cloudShadowTexture);
		glBindTexture(GL_TEXTURE_2D_ARRAY, _cloudShadowTexture);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 10, GL_R16F, 512, 512, 8);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float border_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_color);

		// 2D Cloud Properties LUT: 64x64 RGBA16F
		// X = Cloud Type [0.0 = Cumulonimbus, 0.5 = Cumulus, 1.0 = Stratus]
		// Y = Relative step height h [0.0, 1.0]
		glGenTextures(1, &_cloud2DPropsLUT);
		glBindTexture(GL_TEXTURE_2D, _cloud2DPropsLUT);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, 64, 64);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		std::vector<glm::vec4> lut2DData(64 * 64);
		for (int y = 0; y < 64; ++y) {
			float h = static_cast<float>(y) / 63.0f;
			for (int x = 0; x < 64; ++x) {
				float type = static_cast<float>(x) / 63.0f;

				// R: Density height gradient
				float cumulonimbus = glm::smoothstep(0.0f, 0.05f, h) * (1.0f - glm::smoothstep(0.7f, 1.0f, h));
				float cumulus = glm::smoothstep(0.0f, 0.2f, h) * (1.0f - glm::smoothstep(0.6f, 0.9f, h));
				float stratus = glm::smoothstep(0.0f, 0.3f, h) * (1.0f - glm::smoothstep(0.7f, 1.0f, h));

				float densityGrad = glm::mix(cumulonimbus, cumulus, glm::smoothstep(0.0f, 0.5f, type));
				densityGrad = glm::mix(densityGrad, stratus, glm::smoothstep(0.5f, 1.0f, type));

				// G: Anvil bias (enhanced top spread for cumulonimbus clouds)
				float anvilBiasCb = glm::mix(0.5f, 1.0f, glm::smoothstep(0.5f, 0.95f, h));
				float anvilBiasCu = 0.5f;
				float anvilBiasSt = 0.1f;
				float anvilBias = glm::mix(anvilBiasCb, anvilBiasCu, glm::smoothstep(0.0f, 0.5f, type));
				anvilBias = glm::mix(anvilBias, anvilBiasSt, glm::smoothstep(0.5f, 1.0f, type));

				// B: Base shape / noise blend factor
				float noiseBlendCb = 1.2f;
				float noiseBlendCu = 1.0f;
				float noiseBlendSt = 0.7f;
				float noiseBlend = glm::mix(noiseBlendCb, noiseBlendCu, glm::smoothstep(0.0f, 0.5f, type));
				noiseBlend = glm::mix(noiseBlend, noiseBlendSt, glm::smoothstep(0.5f, 1.0f, type));

				// A: Erosion multiplier
				float erosionCb = 1.4f;
				float erosionCu = 1.0f;
				float erosionSt = 0.4f;
				float erosion = glm::mix(erosionCb, erosionCu, glm::smoothstep(0.0f, 0.5f, type));
				erosion = glm::mix(erosion, erosionSt, glm::smoothstep(0.5f, 1.0f, type));

				lut2DData[y * 64 + x] = glm::vec4(densityGrad, anvilBias, noiseBlend, erosion);
			}
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA, GL_FLOAT, lut2DData.data());

		// 3D Weather Front LUT: 32x32x32 RGBA16F
		// X = Wind speed [0.0, 1.0]
		// Y = Temperature [0.0, 1.0]
		// Z = Humidity [0.0, 1.0]
		glGenTextures(1, &_cloud3DFrontLUT);
		glBindTexture(GL_TEXTURE_3D, _cloud3DFrontLUT);
		glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, 32, 32, 32);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		std::vector<glm::vec4> lut3DData(32 * 32 * 32);
		for (int z = 0; z < 32; ++z) {
			float hum = static_cast<float>(z) / 31.0f;
			for (int y = 0; y < 32; ++y) {
				float temp = static_cast<float>(y) / 31.0f;
				for (int x = 0; x < 32; ++x) {
					float wind = static_cast<float>(x) / 31.0f;

					// R: Cloud type (0.0 = Cumulonimbus, 0.5 = Cumulus, 1.0 = Stratus / Thin)
					float stormFactor = glm::clamp(hum * 1.5f * temp * (0.5f + 0.5f * wind), 0.0f, 1.0f);
					float type = 1.0f - stormFactor;

					// G: Coverage / density boost modifier
					float densityBoost = 0.5f + hum * 1.0f + wind * 0.3f;

					// B: Front altitude / thickness modifier
					float thicknessMod = 0.8f + temp * hum * 1.0f;

					// A: Front turbulence / erosion scale modifier
					float turbulence = 0.7f + wind * 0.8f + temp * 0.3f;

					lut3DData[(z * 32 + y) * 32 + x] = glm::vec4(type, densityBoost, thicknessMod, turbulence);
				}
			}
		}
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 32, 32, 32, GL_RGBA, GL_FLOAT, lut3DData.data());

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
		reg.PublishTexture(Constants::TextureUnit::CloudWeatherMinMax(), _cloudWeatherMinMaxTexture);
		reg.PublishTexture(Constants::TextureUnit::Cloud3D(), _cloudVolumeTexture, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::CloudShadowMap(), _cloudShadowTexture, GL_TEXTURE_2D_ARRAY);
		reg.PublishTexture(Constants::TextureUnit::Cloud2DProps(), _cloud2DPropsLUT, GL_TEXTURE_2D);
		reg.PublishTexture(Constants::TextureUnit::Cloud3DFront(), _cloud3DFrontLUT, GL_TEXTURE_3D);
	}

	void AtmosphereManager::CreateShaders() {
		auto setup_shader = [](ComputeShader& s) {
			s.use();
			s.bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
			s.bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
			s.bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
		};

		_transmittanceShader = std::make_unique<ComputeShader>("shaders/atmosphere/transmittance_lut.comp");
		_multiScatteringShader = std::make_unique<ComputeShader>("shaders/atmosphere/multiscattering_lut.comp");
		_skyViewShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_view_lut.comp");
		_aerialPerspectiveShader = std::make_unique<ComputeShader>("shaders/atmosphere/aerial_perspective_lut.comp");
		_skyToSHShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_to_sh.comp");
		_cloudBakeShader = std::make_unique<ComputeShader>("shaders/effects/cloud_weather_bake.comp");
		_cloudVolumeBakeShader = std::make_unique<ComputeShader>("shaders/effects/cloud_3d_volume_bake.comp");
		_cloudMipShader = std::make_unique<ComputeShader>("shaders/effects/cloud_weather_mip.comp");
		_cloudShadowBakeShader = std::make_unique<ComputeShader>("shaders/effects/cloud_shadow_bake.comp");
		_cloudPaintShader = std::make_unique<ComputeShader>("shaders/effects/cloud_weather_paint.comp");
		_cloudMinMaxInitShader = std::make_unique<ComputeShader>("shaders/effects/cloud_weather_minmax_init.comp");

		setup_shader(*_transmittanceShader);
		setup_shader(*_multiScatteringShader);
		setup_shader(*_skyViewShader);
		setup_shader(*_aerialPerspectiveShader);
		setup_shader(*_skyToSHShader);
		setup_shader(*_cloudBakeShader);
		setup_shader(*_cloudVolumeBakeShader);
		setup_shader(*_cloudShadowBakeShader);
	}

	void AtmosphereManager::Update(
		const glm::vec3& sunDir,
		const glm::vec3& sunColor,
		float            sunIntensity,
		const glm::vec3& cameraPos,
		float            time,
		float            worldScale,
		const glm::vec3& moonDir,
		const glm::vec3& moonRadiance
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

		if (_needsWeatherBake && !_useCustomWeatherMap) {
			// Clear seeds buffer before bake
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _cloudSeedsBuffer);
			std::vector<glm::vec4> clearData(100, glm::vec4(0, 0, 100000.0f, 0));
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 100 * sizeof(glm::vec4), clearData.data());

			// 1. Dispatch 2D weather map bake and initial min-max density map
			_cloudBakeShader->use();
			_cloudBakeShader->setFloat("uCloudCoverage", _cloudCoverage);
			_cloudBakeShader->setFloat("uCloudThickness", _cloudThickness);
			_cloudBakeShader->setFloat("uWorldScale", worldScale);
			glBindImageTexture(0, _cloudWeatherTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindImageTexture(1, _cloudWeatherMinMaxTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::CloudSeeds(), _cloudSeedsBuffer);
			GpuResourceRegistry::Instance().BindTextures({Constants::TextureUnit::NoiseExtra()});
			glDispatchCompute(2048 / 16, 2048 / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Generate mipmaps for the weather map
			glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);

			// 2. Generate mipmaps for the weather min-max density map using custom min-max-downsampling compute shader
			_cloudMipShader->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
			_cloudMipShader->setInt("u_srcWeatherMinMaxMap", 0);

			for (int dstLevel = 1; dstLevel < 12; ++dstLevel) {
				int srcLevel = dstLevel - 1;
				int dstWidth = std::max(1, 2048 >> dstLevel);
				int dstHeight = std::max(1, 2048 >> dstLevel);

				glBindImageTexture(0, _cloudWeatherMinMaxTexture, dstLevel, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
				_cloudMipShader->setInt("u_srcLevel", srcLevel);

				glDispatchCompute((dstWidth + 7) / 8, (dstHeight + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
			}
			glBindTexture(GL_TEXTURE_2D, 0);

			// 3. Dispatch 3D volume bake, sampling the baked weather map
			_cloudVolumeBakeShader->use();
			_cloudVolumeBakeShader->setInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
			glBindImageTexture(0, _cloudVolumeTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherBake());
			glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);

			GpuResourceRegistry::Instance().BindTextures({
				Constants::TextureUnit::NoiseSimplex(),
				Constants::TextureUnit::NoiseCurl(),
				Constants::TextureUnit::NoiseBlue(),
				Constants::TextureUnit::NoiseExtra(),
				Constants::TextureUnit::NoisePhasor()
			});
			glDispatchCompute(128 / 4, 128 / 4, 128 / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Generate mipmaps for the weather map
			glBindTexture(GL_TEXTURE_3D, _cloudVolumeTexture);
			glGenerateMipmap(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, 0);

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

			_needsWeatherBake = false;
			_worldScale = worldScale;
		}

		// Cloud Shadow Map Bake
		auto light_mgr = ServiceLocator::Instance().Get<LightManager>();
		glm::vec3 primaryLightDir = sunDir;
		if (light_mgr) {
			const auto& lights = light_mgr->GetLights();
			float max_intensity = -1.0f;
			for (const auto& light : lights) {
				if (light.type == Boidsish::DIRECTIONAL_LIGHT) {
					if (light.intensity > max_intensity) {
						max_intensity = light.intensity;
						primaryLightDir = glm::normalize(-light.direction);
					}
				}
			}
		}

		if (_enableCloudShadowMap && _cloudShadowBakeShader && _cloudShadowBakeShader->isValid()) {
			glm::vec3 center = glm::vec3(cameraPos.x, 0.0f, cameraPos.z);
			glm::vec3 lightDir = glm::normalize(primaryLightDir);
			glm::vec3 lightPos = center + lightDir * (20000.0f * worldScale);
			glm::vec3 target = center - lightDir * (20000.0f * worldScale);
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			if (std::abs(lightDir.y) > 0.9f) {
				up = glm::vec3(0.0f, 0.0f, 1.0f);
			}
			glm::mat4 lightView = glm::lookAt(lightPos, target, up);
			float half_ext = 100000.0f * worldScale;
			glm::mat4 lightProj = glm::ortho(-half_ext, half_ext, -half_ext, half_ext, 0.0f, 40000.0f * worldScale);
			_cloudShadowMatrix = lightProj * lightView;
			_cloudShadowInvMatrix = glm::inverse(_cloudShadowMatrix);

			_lastBakedLightDir = primaryLightDir;
			_lastBakedCameraPos = cameraPos;
			_lastBakedCloudCoverage = _cloudCoverage;
			_lastBakedCloudDensity = _cloudDensity;
			_lastBakedCloudAltitude = _cloudAltitude;
			_lastBakedCloudThickness = _cloudThickness;

			_cloudShadowBakeShader->use();
			BindToShader(*_cloudShadowBakeShader);
			_cloudShadowBakeShader->setMat4("u_lightSpaceMatrix", _cloudShadowMatrix);
			_cloudShadowBakeShader->setMat4("u_invLightSpaceMatrix", _cloudShadowInvMatrix);
			_cloudShadowBakeShader->setVec3("u_primaryLightDir", primaryLightDir);
			_cloudShadowBakeShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
			_cloudShadowBakeShader->setFloat("u_time", time);
			_cloudShadowBakeShader->setFloat("u_cloudCoverage", _cloudCoverage);
			_cloudShadowBakeShader->setFloat("u_worldScale", worldScale);
			_cloudShadowBakeShader->setFloat("u_cloudAltitude", _cloudAltitude);
			_cloudShadowBakeShader->setFloat("u_cloudThickness", _cloudThickness);
			_cloudShadowBakeShader->setFloat("u_cloudDensity", _cloudDensity);
			_cloudShadowBakeShader->setInt("u_frameIndex", _frameIndex);

			glBindImageTexture(0, _cloudShadowTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R16F);

			glDispatchCompute(512 / 8, 512 / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

			// Generate mipmaps for blurred lookups (soft shadows/AO)
			glBindTexture(GL_TEXTURE_2D_ARRAY, _cloudShadowTexture);
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}
		_frameIndex++;

		// Dispatch SkyView
		_skyViewShader->use();
		glBindImageTexture(0, _skyViewLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		BindToShader(*_skyViewShader);

		_skyViewShader->setVec3("u_sunDir", sunDir);
		_skyViewShader->setVec3("u_sunRadiance", sunColor * sunIntensity);
		_skyViewShader->setVec3("u_moonDir", moonDir);
		_skyViewShader->setVec3("u_moonRadiance", moonRadiance);
		_skyViewShader->setVec3("u_cameraPos", cameraPos);
		_skyViewShader->setFloat("u_time", time);
		_skyViewShader->setFloat("u_rayleighScale", _rayleighScale);
		_skyViewShader->setFloat("u_mieScale", _mieScale);
		_skyViewShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
		_skyViewShader->setFloat("u_multiScatScale", _multiScatScale);

		_skyViewShader->setFloat("u_worldScale", worldScale);
		_skyViewShader->setFloat("u_cloudShadowIntensity", _cloudShadowIntensity);

		_skyViewShader->setFloat("u_atmosphereHeight", _atmosphereHeight);
		_skyViewShader->setVec3("u_rayleighScatteringBase", _rayleighScattering);
		_skyViewShader->setFloat("u_mieScatteringBase", _mieScattering);
		_skyViewShader->setFloat("u_mieExtinctionBase", _mieExtinction);
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
		BindToShader(*_aerialPerspectiveShader);

		_aerialPerspectiveShader->setVec3("u_sunDir", sunDir);
		_aerialPerspectiveShader->setVec3("u_sunRadiance", sunColor * sunIntensity);
		_aerialPerspectiveShader->setVec3("u_moonDir", moonDir);
		_aerialPerspectiveShader->setVec3("u_moonRadiance", moonRadiance);
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
		_aerialPerspectiveShader->setFloat("u_sunAureoleStrength", _sunAureoleStrength);
		_aerialPerspectiveShader->setFloat("u_cirrusOpacity", _cirrusOpacity);

		_aerialPerspectiveShader->setFloat("hazeDensity", weather.haze_density);
		_aerialPerspectiveShader->setFloat("hazeHeight", weather.haze_height);
		_aerialPerspectiveShader->setVec3("hazeColor", weather.haze_color);

		glDispatchCompute(32 / 4, 32 / 4, 32 / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Dispatch SkyToSH
		_skyToSHShader->use();
		BindToShader(*_skyToSHShader);
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
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherMinMax());
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::Cloud3D());
		glBindTexture(GL_TEXTURE_3D, _cloudVolumeTexture);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudShadowMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, _cloudShadowTexture);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::Cloud2DProps());
		glBindTexture(GL_TEXTURE_2D, _cloud2DPropsLUT);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::Cloud3DFront());
		glBindTexture(GL_TEXTURE_3D, _cloud3DFrontLUT);
	}

	void AtmosphereManager::BindToShader(::ShaderBase& shader) {
		BindTextures();
		shader.bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
		shader.trySetInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());
		shader.trySetInt("u_multiScatteringLUT", Constants::TextureUnit::AtmosphereMultiScattering());
		shader.trySetInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());
		shader.trySetInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());
		shader.trySetInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
		shader.trySetInt("u_cloudWeatherMinMaxTexture", Constants::TextureUnit::CloudWeatherMinMax());
		shader.trySetInt("u_cloud3DTexture", Constants::TextureUnit::Cloud3D());
		shader.trySetInt("u_cloudShadowTexture", Constants::TextureUnit::CloudShadowMap());
		shader.trySetInt("u_cloud2DPropsLUT", Constants::TextureUnit::Cloud2DProps());
		shader.trySetInt("u_cloud3DFrontLUT", Constants::TextureUnit::Cloud3DFront());
		shader.setMat4("u_cloudShadowMatrix", _cloudShadowMatrix);
		shader.setBool("u_useCloudShadowMap", _enableCloudShadowMap);
		shader.trySetFloat("u_cloudShadowIntensity", _cloudShadowIntensity);
		shader.trySetFloat("u_atmosphereHeight", _atmosphereHeight);

		shader.setVec3("u_rayleighScatteringBase", _rayleighScattering);
		shader.trySetFloat("u_rayleighScaleHeight", _rayleighScaleHeight);
		shader.trySetFloat("u_mieScatteringBase", _mieScattering);
		shader.trySetFloat("u_mieExtinctionBase", _mieExtinction);
		shader.trySetFloat("u_mieScaleHeight", _mieScaleHeight);
		shader.trySetFloat("u_rayleighScale", _rayleighScale);
		shader.trySetFloat("u_mieScale", _mieScale);
		shader.trySetFloat("u_mieAnisotropy", _mieAnisotropy);
		shader.setVec3("u_ozoneAbsorptionBase", _ozoneAbsorption);
	}

	glm::vec4 AtmosphereManager::GetCloudWeather(glm::vec2 worldXZ, float time) const {
		if (_cpuWeatherMap.empty()) return glm::vec4(100000.0f, 0, 0, 0);

		// factor in advection (matches shaders/helpers/cloud_utils.glsl computeCloudWeather)
		float     angle = _cloudFlowDirection;
		glm::vec2 flowDir = glm::vec2(cos(angle), sin(angle));
		glm::vec2 advect = flowDir * _cloudFlowSpeed * _worldScale * 10.0f * time;

		glm::vec2 p_advected = worldXZ - advect;
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

	void AtmosphereManager::SetUseCustomWeatherMap(bool b) {
		if (_useCustomWeatherMap != b) {
			_useCustomWeatherMap = b;
			if (!b) {
				_needsWeatherBake = true;
			}
		}
	}

	bool AtmosphereManager::ExportCloudWeatherMap(const std::string& filepath) {
		if (!_cloudWeatherTexture) {
			std::cerr << "[AtmosphereManager] Cannot export, texture not initialized." << std::endl;
			return false;
		}

		std::vector<glm::vec4> floatPixels(2048 * 2048);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, floatPixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		std::vector<unsigned char> bytePixels(2048 * 2048 * 4);
		for (size_t i = 0; i < floatPixels.size(); ++i) {
			bytePixels[i * 4 + 0] = static_cast<unsigned char>(glm::clamp(floatPixels[i].r, 0.0f, 1.0f) * 255.0f + 0.5f);
			bytePixels[i * 4 + 1] = static_cast<unsigned char>(glm::clamp(floatPixels[i].g, 0.0f, 1.0f) * 255.0f + 0.5f);
			bytePixels[i * 4 + 2] = static_cast<unsigned char>(glm::clamp(floatPixels[i].b, 0.0f, 1.0f) * 255.0f + 0.5f);
			bytePixels[i * 4 + 3] = static_cast<unsigned char>(glm::clamp(floatPixels[i].a, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		int success = stbi_write_png(filepath.c_str(), 2048, 2048, 4, bytePixels.data(), 2048 * 4);
		if (!success) {
			std::cerr << "[AtmosphereManager] Failed to write PNG to " << filepath << std::endl;
			return false;
		}

		std::cout << "[AtmosphereManager] Successfully exported cloud weather map to " << filepath << std::endl;
		return true;
	}

	bool AtmosphereManager::ImportCloudWeatherMap(const std::string& filepath) {
		int width = 0, height = 0, channels = 0;
		unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
		if (!data) {
			std::cerr << "[AtmosphereManager] Failed to load image from " << filepath << std::endl;
			return false;
		}

		std::vector<glm::vec4> floatPixels(2048 * 2048);
		for (int y = 0; y < 2048; ++y) {
			for (int x = 0; x < 2048; ++x) {
				float srcX = (static_cast<float>(x) + 0.5f) / 2048.0f * static_cast<float>(width);
				float srcY = (static_cast<float>(y) + 0.5f) / 2048.0f * static_cast<float>(height);
				int ix = std::clamp(static_cast<int>(srcX), 0, width - 1);
				int iy = std::clamp(static_cast<int>(srcY), 0, height - 1);
				int index = (iy * width + ix) * 4;
				float r = data[index + 0] / 255.0f;
				float g = data[index + 1] / 255.0f;
				float b = data[index + 2] / 255.0f;
				float a = data[index + 3] / 255.0f;
				floatPixels[y * 2048 + x] = glm::vec4(r, g, b, a);
			}
		}
		stbi_image_free(data);

		// Activate custom weather map mode
		_useCustomWeatherMap = true;

		// 1. Upload weather map to Level 0 of _cloudWeatherTexture
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2048, 2048, GL_RGBA, GL_FLOAT, floatPixels.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// 2. Generate min-max texture Level 0 from R-channel (dist/coverage)
		std::vector<glm::vec2> minMaxData(2048 * 2048);
		for (size_t i = 0; i < floatPixels.size(); ++i) {
			float f1_dist = floatPixels[i].r;
			float coverage = 1.0f - f1_dist;
			float finalCoverage = glm::clamp(coverage, 0.0f, 1.0f);
			float t = glm::clamp((finalCoverage - 0.05f) / (1.0f - 0.05f), 0.0f, 1.0f);
			finalCoverage = t * t * (3.0f - 2.0f * t);
			minMaxData[i] = glm::vec2(finalCoverage, finalCoverage);
		}
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2048, 2048, GL_RG, GL_FLOAT, minMaxData.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		// 3. Rerun the custom min-max mipmap downsampling shader
		_cloudMipShader->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
		_cloudMipShader->setInt("u_srcWeatherMinMaxMap", 0);

		for (int dstLevel = 1; dstLevel < 12; ++dstLevel) {
			int srcLevel = dstLevel - 1;
			int dstWidth = std::max(1, 2048 >> dstLevel);
			int dstHeight = std::max(1, 2048 >> dstLevel);

			glBindImageTexture(0, _cloudWeatherMinMaxTexture, dstLevel, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
			_cloudMipShader->setInt("u_srcLevel", srcLevel);

			glDispatchCompute((dstWidth + 7) / 8, (dstHeight + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// 4. Rerun the 3D volume bake shader
		_cloudVolumeBakeShader->use();
		_cloudVolumeBakeShader->setInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
		glBindImageTexture(0, _cloudVolumeTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherBake());
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);

		GpuResourceRegistry::Instance().BindTextures({
			Constants::TextureUnit::NoiseSimplex(),
			Constants::TextureUnit::NoiseCurl(),
			Constants::TextureUnit::NoiseBlue(),
			Constants::TextureUnit::NoiseExtra(),
			Constants::TextureUnit::NoisePhasor()
		});
		glDispatchCompute(128 / 4, 128 / 4, 128 / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Generate mipmaps for 3D cloud volume
		glBindTexture(GL_TEXTURE_3D, _cloudVolumeTexture);
		glGenerateMipmap(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, 0);

		// 5. Update CPU weather map for CPU/gameplay queries
		_cpuWeatherMap = floatPixels;

		std::cout << "[AtmosphereManager] Successfully imported custom cloud weather map from " << filepath << std::endl;
		return true;
	}

	void AtmosphereManager::PaintWeatherMap(
		const glm::vec2& brushCenterUV,
		float            brushRadiusUV,
		const glm::vec4& brushValue,
		const glm::vec4& channelMask,
		int              drawOp,
		float            brushStrength,
		float            smoothness
	) {
		if (!_cloudPaintShader || !_cloudPaintShader->isValid() || !_cloudWeatherTexture) return;

		_useCustomWeatherMap = true;

		_cloudPaintShader->use();
		_cloudPaintShader->setVec2("u_brushCenter", brushCenterUV);
		_cloudPaintShader->setFloat("u_brushRadius", brushRadiusUV);
		_cloudPaintShader->setVec4("u_brushValue", brushValue);
		_cloudPaintShader->setVec4("u_channelMask", channelMask);
		_cloudPaintShader->setInt("u_drawOp", drawOp);
		_cloudPaintShader->setFloat("u_brushStrength", brushStrength);
		_cloudPaintShader->setFloat("u_smoothness", smoothness);

		glBindImageTexture(0, _cloudWeatherTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
		glDispatchCompute(2048 / 16, 2048 / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		RebakeWeatherMinMaxAndVolume();
	}

	void AtmosphereManager::RebakeWeatherMinMaxAndVolume() {
		if (!_cloudMinMaxInitShader || !_cloudMipShader || !_cloudVolumeBakeShader) return;

		// 1. Re-initialize Level 0 of _cloudWeatherMinMaxTexture from R channel of _cloudWeatherTexture
		_cloudMinMaxInitShader->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		_cloudMinMaxInitShader->setInt("u_weatherMap", 0);
		glBindImageTexture(0, _cloudWeatherMinMaxTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
		glDispatchCompute(2048 / 16, 2048 / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		// 2. Generate mipmaps for _cloudWeatherTexture
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glGenerateMipmap(GL_TEXTURE_2D);

		// 3. Dispatch _cloudMipShader across min-max pyramid (dstLevel 1 to 11)
		_cloudMipShader->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherMinMaxTexture);
		_cloudMipShader->setInt("u_srcWeatherMinMaxMap", 0);

		for (int dstLevel = 1; dstLevel < 12; ++dstLevel) {
			int srcLevel = dstLevel - 1;
			int dstWidth = std::max(1, 2048 >> dstLevel);
			int dstHeight = std::max(1, 2048 >> dstLevel);

			glBindImageTexture(0, _cloudWeatherMinMaxTexture, dstLevel, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
			_cloudMipShader->setInt("u_srcLevel", srcLevel);

			glDispatchCompute((dstWidth + 7) / 8, (dstHeight + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// 4. Dispatch 3D volume bake
		_cloudVolumeBakeShader->use();
		_cloudVolumeBakeShader->setInt("u_cloudWeatherTexture", Constants::TextureUnit::CloudWeatherBake());
		glBindImageTexture(0, _cloudVolumeTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::CloudWeatherBake());
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);

		GpuResourceRegistry::Instance().BindTextures({
			Constants::TextureUnit::NoiseSimplex(),
			Constants::TextureUnit::NoiseCurl(),
			Constants::TextureUnit::NoiseBlue(),
			Constants::TextureUnit::NoiseExtra(),
			Constants::TextureUnit::NoisePhasor()
		});
		glDispatchCompute(128 / 4, 128 / 4, 128 / 4);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Generate mipmaps for 3D cloud volume
		glBindTexture(GL_TEXTURE_3D, _cloudVolumeTexture);
		glGenerateMipmap(GL_TEXTURE_3D);
		glBindTexture(GL_TEXTURE_3D, 0);

		// 5. Update CPU weather map readback
		_cpuWeatherMap.resize(2048 * 2048);
		glBindTexture(GL_TEXTURE_2D, _cloudWeatherTexture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, _cpuWeatherMap.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void AtmosphereManager::ForceRebakeWeatherMap() {
		_useCustomWeatherMap = false;
		_needsWeatherBake = true;
	}

	void AtmosphereManager::ClearWeatherMap() {
		if (!_cloudWeatherTexture) return;

		_useCustomWeatherMap = true;

		// Clear cloud weather map Level 0 to zero
		float zeroColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glClearTexImage(_cloudWeatherTexture, 0, GL_RGBA, GL_FLOAT, zeroColor);

		RebakeWeatherMinMaxAndVolume();
	}

} // namespace Boidsish
