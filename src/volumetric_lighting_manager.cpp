#include "volumetric_lighting_manager.h"
#include "service_locator.h"
#include "shader.h"
#include "constants.h"
#include "NoiseManager.h"
#include "shadow_manager.h"
#include "weather_manager.h"
#include "light_manager.h"
#include "fire_effect_manager.h"
#include "atmosphere_manager.h"
#include "profiler.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

    VolumetricLightingManager::VolumetricLightingManager(ServiceLocator& loc) : _loc(loc) {
        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            _densityVolumes[i] = 0;
            _scatteringVolumes[i] = 0;
            _integratedVolumes[i] = 0;
        }
    }

    VolumetricLightingManager::~VolumetricLightingManager() {
        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            if (_densityVolumes[i]) glDeleteTextures(1, &_densityVolumes[i]);
            if (_scatteringVolumes[i]) glDeleteTextures(1, &_scatteringVolumes[i]);
            if (_integratedVolumes[i]) glDeleteTextures(1, &_integratedVolumes[i]);
        }
        if (_parameterUbo) glDeleteBuffers(1, &_parameterUbo);
    }

    void VolumetricLightingManager::Initialize() {
        CreateTextures();
        CreateShaders();
        CreateBuffers();
    }

    void VolumetricLightingManager::CreateTextures() {
        auto create3DTexture = [&](GLuint& tex, GLenum internalFormat, int w, int h, int d) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_3D, tex);
            glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, w, h, d, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        };

        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            // Distant cascades can have lower resolution
            int div = 1 << i;
            create3DTexture(_densityVolumes[i], GL_R16F, _gridW / div, _gridH / div, _gridD);
            create3DTexture(_scatteringVolumes[i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
            create3DTexture(_integratedVolumes[i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
        }

        glBindTexture(GL_TEXTURE_3D, 0);
    }

    void VolumetricLightingManager::CreateShaders() {
        _gridInitShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_grid_init.comp");
        _densityVoxelizationShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_density.comp");
        _lightingInjectionShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_lighting_injection.comp");
        _integrationShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_integration.comp");
    }

    void VolumetricLightingManager::CreateBuffers() {
        glGenBuffers(1, &_parameterUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, _parameterUbo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(VolumetricLightingUbo), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void VolumetricLightingManager::Update(float deltaTime, float totalTime, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
        auto weatherMgr = _loc.Get<WeatherManager>();
        auto lightMgr = _loc.Get<LightManager>();
        auto atmosphereMgr = _loc.Get<AtmosphereManager>();

        VolumetricLightingUbo ubo{};
        glm::mat4 viewProj = projection * view;
        ubo.invViewProj = glm::inverse(viewProj);
        ubo.prevViewProj = _prevViewProj;

        ubo.gridParams = glm::vec4(_nearPlane, _farPlane, 1.0f, (float)kMaxVolumetricCascades);
        ubo.resolution = glm::vec4((float)_gridW, (float)_gridH, (float)_gridD, 1.0f);

        // Cascaded Splits (matches shadow manager for consistency)
        ubo.cascadeSplits = glm::vec4(20.0f, 50.0f, 150.0f, 700.0f);

        const auto& lights = lightMgr->GetLights();
        if (!lights.empty()) {
            ubo.sunDir = glm::vec4(-lights[0].direction, lights[0].intensity);
            ubo.sunColor = glm::vec4(lights[0].color, atmosphereMgr->GetMieAnisotropy());
        }

        const auto& weather = weatherMgr->GetCurrentWeather();
        ubo.hazeParams = glm::vec4(weather.haze_density, weather.haze_height, 0.01f, 0.5f);
        ubo.ambientColor = glm::vec4(atmosphereMgr->GetAmbientEstimate(), 1.0f);
        ubo.cloudParams = glm::vec4(weather.cloud_coverage, weather.cloud_density, 0.5f, 0.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, _parameterUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VolumetricLightingUbo), &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        _prevViewProj = viewProj;
    }

    void VolumetricLightingManager::Dispatch(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float totalTime) {
        PROJECT_PROFILE_SCOPE("VolumetricLighting::Dispatch");

        // Bind UBO for parameters
        glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), _parameterUbo, 0, sizeof(VolumetricLightingUbo));

        auto weatherMgr = _loc.Get<WeatherManager>();
        auto noiseMgr = _loc.Get<NoiseManager>();
        auto shadowMgr = _loc.Get<ShadowManager>();
        auto lightMgr = _loc.Get<LightManager>();

        for (int cascade = 0; cascade < kMaxVolumetricCascades; ++cascade) {
            int div = 1 << cascade;
            int cw = _gridW / div;
            int ch = _gridH / div;
            int cd = _gridD;

            _densityVoxelizationShader->use();
            _densityVoxelizationShader->setInt("u_cascadeIndex", cascade);
            glBindImageTexture(0, _densityVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R16F);
            if (noiseMgr) noiseMgr->BindDefault(*_densityVoxelizationShader);
            if (weatherMgr) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::WindData() + 1);
                glBindTexture(GL_TEXTURE_2D, weatherMgr->GetAerosolTexture());
                _densityVoxelizationShader->trySetInt("u_aerosolTexture", Constants::TextureUnit::WindData() + 1);
            }
            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, (cd + 7) / 8);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            _lightingInjectionShader->use();
            _lightingInjectionShader->setInt("u_cascadeIndex", cascade);
            glBindImageTexture(0, _scatteringVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _densityVolumes[cascade]);
            _lightingInjectionShader->setInt("u_densityVolume", 1);
            if (weatherMgr) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, weatherMgr->GetAerosolTexture());
                _lightingInjectionShader->trySetInt("u_aerosolTexture", 2);
            }
            if (shadowMgr && lightMgr) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMaps());
                glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMgr->GetShadowMapArray());
                _lightingInjectionShader->trySetInt("shadowMaps", Constants::TextureUnit::ShadowMaps());
            }
            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, (cd + 7) / 8);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

            _integrationShader->use();
            _integrationShader->setInt("u_cascadeIndex", cascade);
            glBindImageTexture(0, _integratedVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _scatteringVolumes[cascade]);
            _integrationShader->setInt("u_scatteringVolume", 1);
            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    void VolumetricLightingManager::BindToShader(class ShaderBase& shader) {
        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            std::string name = "u_volumetricIntegrated[" + std::to_string(i) + "]";
            glActiveTexture(GL_TEXTURE0 + 28 + i);
            glBindTexture(GL_TEXTURE_3D, _integratedVolumes[i]);
            shader.trySetInt(name, 28 + i);
        }

        // Also bind the VolumetricLighting UBO
        GLuint ubo_idx = glGetUniformBlockIndex(shader.ID, "VolumetricLighting");
        if (ubo_idx != GL_INVALID_INDEX) {
            glUniformBlockBinding(shader.ID, ubo_idx, Constants::UboBinding::VolumetricLighting());
        }
    }

    GLuint VolumetricLightingManager::GetIntegratedVolume() const {
        return _integratedVolumes[0]; // Return near cascade by default
    }

} // namespace Boidsish
