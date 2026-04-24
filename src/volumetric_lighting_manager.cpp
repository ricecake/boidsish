#include "volumetric_lighting_manager.h"
#include "service_locator.h"
#include "profiler.h"
#include "shader.h"
#include "constants.h"
#include <iostream>

namespace Boidsish {

	VolumetricLightingManager::VolumetricLightingManager(ServiceLocator& loc) : _loc(loc) {
		for (int c = 0; c < kNumCascades; ++c) {
			for (int b = 0; b < kNumBuffers; ++b) {
				_scatteringVolumes[c][b] = 0;
				_integratedVolumes[c][b] = 0;
			}
		}
	}

	VolumetricLightingManager::~VolumetricLightingManager() {
		for (int c = 0; c < kNumCascades; ++c) {
			glDeleteTextures(kNumBuffers, _scatteringVolumes[c]);
			glDeleteTextures(kNumBuffers, _integratedVolumes[c]);
		}
	}

	void VolumetricLightingManager::Initialize() {
		CreateTextures();
		CreateShaders();
		_ubo = std::make_unique<PersistentBuffer<VolumetricLightingUbo>>(GL_UNIFORM_BUFFER, 1, kNumBuffers);
	}

	void VolumetricLightingManager::CreateTextures() {
		for (int c = 0; c < kNumCascades; ++c) {
			glGenTextures(kNumBuffers, _scatteringVolumes[c]);
			glGenTextures(kNumBuffers, _integratedVolumes[c]);

			for (int b = 0; b < kNumBuffers; ++b) {
				// Scattering & Extinction volume (RGBA16F: RGB = scattering, A = extinction)
				glBindTexture(GL_TEXTURE_3D, _scatteringVolumes[c][b]);
				glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, _resX, _resY, _resZ, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

				// Integrated Scattering & Transmittance volume
				glBindTexture(GL_TEXTURE_3D, _integratedVolumes[c][b]);
				glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, _resX, _resY, _resZ, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}
		}
		glBindTexture(GL_TEXTURE_3D, 0);
	}

	void VolumetricLightingManager::CreateShaders() {
		_densityShader = std::make_unique<ComputeShader>("shaders/volumetric_density.comp");
		_injectionShader = std::make_unique<ComputeShader>("shaders/volumetric_injection.comp");
		_integrationShader = std::make_unique<ComputeShader>("shaders/volumetric_integration.comp");

		auto setup_bindings = [](ComputeShader& s) {
			s.use();
			GLuint lighting_idx = glGetUniformBlockIndex(s.ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(s.ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint shadows_idx = glGetUniformBlockIndex(s.ID, "Shadows");
			if (shadows_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(s.ID, shadows_idx, Constants::UboBinding::Shadows());
			}
			GLuint vol_idx = glGetUniformBlockIndex(s.ID, "VolumetricLighting");
			if (vol_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(s.ID, vol_idx, Constants::UboBinding::VolumetricLighting());
			}
		};

		setup_bindings(*_densityShader);
		setup_bindings(*_injectionShader);
		setup_bindings(*_integrationShader);
	}

	void VolumetricLightingManager::UpdateUbo() {
		VolumetricLightingUbo data{};
		data.cascadeRanges = glm::vec4(0.1f, _cascade0Far, _cascade0Far, _cascade1Far);
		data.cascadeRes = glm::ivec4(_resX, _resY, _resZ, 0);
		data.intensity = _intensity;
		data.scatteringCoeff = _scatteringCoeff;
		data.extinctionCoeff = _extinctionCoeff;
		data.mieAnisotropy = _mieAnisotropy;
		data.ambientFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		data.phaseG = _mieAnisotropy;
		data.weatherGridOriginSize = _weatherGridOriginSize;

		*_ubo->GetFrameDataPtr() = data;
		glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(),
			_ubo->GetBufferId(), _ubo->GetFrameOffset(), sizeof(VolumetricLightingUbo));
	}

	void VolumetricLightingManager::AdvanceFrame() {
		_frameIndex = (_frameIndex + 1) % kNumBuffers;
		_ubo->AdvanceFrame();
	}

	void VolumetricLightingManager::Update(
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPos,
		float deltaTime,
		float time,
		GLuint weatherScalarTexture,
		const glm::ivec4& weatherGridOriginSize
	) {
		PROJECT_PROFILE_SCOPE("VolumetricLighting::Update");

		_weatherScalarTexture = weatherScalarTexture;
		_weatherGridOriginSize = weatherGridOriginSize;

		UpdateUbo();

		for (int c = 0; c < kNumCascades; ++c) {
			// 1. Voxelize Density
			_densityShader->use();
			_densityShader->setInt("u_cascadeIndex", c);
			_densityShader->setMat4("view", view);
			_densityShader->setMat4("projection", projection);
			_densityShader->setMat4("invView", glm::inverse(view));

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WeatherScalars());
			glBindTexture(GL_TEXTURE_2D, _weatherScalarTexture);
			_densityShader->setInt("u_weatherScalars", Constants::TextureUnit::WeatherScalars());

			glBindImageTexture(0, _scatteringVolumes[c][_frameIndex], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((_resX + 7) / 8, (_resY + 7) / 8, (_resZ + 3) / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Light Injection
			_injectionShader->use();
			_injectionShader->setInt("u_cascadeIndex", c);
			_injectionShader->setMat4("view", view);
			_injectionShader->setMat4("projection", projection);
			_injectionShader->setMat4("invView", glm::inverse(view));
			_injectionShader->setVec3("viewPos", cameraPos);
			glBindImageTexture(0, _scatteringVolumes[c][_frameIndex], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
			glDispatchCompute((_resX + 7) / 8, (_resY + 7) / 8, (_resZ + 3) / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 3. Integration
			_integrationShader->use();
			_integrationShader->setInt("u_cascadeIndex", c);
			glBindImageTexture(0, _scatteringVolumes[c][_frameIndex], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
			glBindImageTexture(1, _integratedVolumes[c][_frameIndex], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute((_resX + 7) / 8, (_resY + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
	}

	void VolumetricLightingManager::BindTextures() {
		for (int c = 0; c < kNumCascades; ++c) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::VolumetricCascades() + c);
			glBindTexture(GL_TEXTURE_3D, _integratedVolumes[c][_frameIndex]);
		}
	}

} // namespace Boidsish
