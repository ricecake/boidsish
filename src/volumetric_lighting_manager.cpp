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
#include "terrain_render_manager.h"
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
        auto create3DTexture = [&](GLuint& tex, GLenum internalFormat, int w, int h, int d, GLenum format = GL_RGBA, GLenum type = GL_FLOAT) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_3D, tex);
            glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, w, h, d, 0, format, type, nullptr);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        };

        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            // Distant cascades can have lower resolution
            int div = 1 << i;
            create3DTexture(_densityVolumes[i], GL_R32UI, _gridW / div, _gridH / div, _gridD, GL_RED_INTEGER, GL_UNSIGNED_INT);
            create3DTexture(_scatteringVolumes[i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
            create3DTexture(_integratedVolumes[i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
        }

        glBindTexture(GL_TEXTURE_3D, 0);
    }

    void VolumetricLightingManager::CreateShaders() {
        _densityVoxelizationShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_density.comp");
        _particleVoxelizationShader = std::make_unique<ComputeShader>("shaders/atmosphere/volumetric_particle_voxelize.comp");
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
        ubo.resolution = glm::vec4((float)_gridW, (float)_gridH, (float)_gridD, _intensity);

        // Cascaded Splits (matches shadow manager for consistency)
        ubo.cascadeSplits = glm::vec4(20.0f, 50.0f, 150.0f, 700.0f);

        const auto& lights = lightMgr->GetLights();
        if (!lights.empty()) {
            ubo.sunDir = glm::vec4(-lights[0].direction, lights[0].intensity);
            ubo.sunColor = glm::vec4(lights[0].color, _phaseG);
        }

        const auto& weather = weatherMgr->GetCurrentWeather();
        ubo.hazeParams = glm::vec4(weather.haze_density, weather.haze_height, 0.01f, 0.5f);
        ubo.ambientColor = glm::vec4(atmosphereMgr->GetAmbientEstimate(), _scatteringScale);
        ubo.cloudParams = glm::vec4(weather.cloud_coverage, weather.cloud_density, 0.5f, 0.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, _parameterUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VolumetricLightingUbo), &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        _prevViewProj = viewProj;
    }

    void VolumetricLightingManager::Dispatch(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float totalTime) {
        PROJECT_PROFILE_SCOPE("VolumetricLighting::Dispatch");

        _lightingInjectionShader->use();
        _lightingInjectionShader->setFloat("u_extinctionScale", _extinctionScale);

        auto weatherMgr = _loc.Get<WeatherManager>();
        auto noiseMgr = _loc.Get<NoiseManager>();
        auto shadowMgr = _loc.Get<ShadowManager>();
        auto lightMgr = _loc.Get<LightManager>();
        auto fireMgr = _loc.Get<FireEffectManager>();

        // Bind UBO for parameters
        glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(), _parameterUbo, 0, sizeof(VolumetricLightingUbo));

        // Bind WindData UBO for world-space aerosol mapping
        weatherMgr->UpdateWindUbo(totalTime); // Ensure updated

        // Bind Shadows UBO for CSM
        if (shadowMgr) {
            glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), shadowMgr->GetShadowUbo(), 0, 16 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4) + 16);
        }

        // Bind Lighting UBO for viewPos
        // (VisualizerImpl usually handles this but we do it here for compute safety)
        // Note: We use the actual buffer handle from the service if needed,
        // but for now we assume it's already bound to LIGHTING_BINDING by VisualizerImpl.

        // Bind SH Probes
        auto terrainMgr = _loc.Get<TerrainRenderManager>();
        if (terrainMgr) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainProbes(), terrainMgr->GetProbeBuffer());
        }

        for (int cascade = 0; cascade < kMaxVolumetricCascades; ++cascade) {
            int div = 1 << cascade;
            int cw = _gridW / div;
            int ch = _gridH / div;
            int cd = _gridD;

            _densityVoxelizationShader->use();
            _densityVoxelizationShader->setInt("u_cascadeIndex", cascade);
            glBindImageTexture(0, _densityVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32UI);
            if (noiseMgr) noiseMgr->BindDefault(*_densityVoxelizationShader);
            if (weatherMgr) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AerosolData());
                glBindTexture(GL_TEXTURE_2D, weatherMgr->GetAerosolTexture());
                _densityVoxelizationShader->trySetInt("u_aerosolTexture", Constants::TextureUnit::AerosolData());
            }
            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, (cd + 7) / 8);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // Splat particles into the density volume
            if (fireMgr) {
                _particleVoxelizationShader->use();
                _particleVoxelizationShader->setInt("u_cascadeIndex", cascade);
                _particleVoxelizationShader->setInt("u_particleCount", Constants::Class::Particles::MaxParticles());
                glBindImageTexture(0, _densityVolumes[cascade], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

                // Particle buffer is already bound to PARTICLE_BUFFER_BINDING in VisualizerImpl::UpdateSystems

                glDispatchCompute((Constants::Class::Particles::MaxParticles() + 255) / 256, 1, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }

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
            int unit = Constants::TextureUnit::VolumetricCascade0() + i;
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_3D, _integratedVolumes[i]);
            shader.trySetInt(name, unit);
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
