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
            // Density volumes MUST use NEAREST filtering for integer textures
            create3DTexture(_densityVolumes[i], GL_R32UI, _gridW / div, _gridH / div, _gridD, GL_RED_INTEGER, GL_UNSIGNED_INT);
            glBindTexture(GL_TEXTURE_3D, _densityVolumes[i]);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            // Use RGBA32F for integration volumes to prevent precision/clamping issues during Z-accumulation
            create3DTexture(_scatteringVolumes[i], GL_RGBA32F, _gridW / div, _gridH / div, _gridD);
            create3DTexture(_integratedVolumes[i], GL_RGBA32F, _gridW / div, _gridH / div, _gridD);
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

    void VolumetricLightingManager::Update(float deltaTime, float totalTime, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float worldScale) {
        auto weatherMgr = _loc.Get<WeatherManager>();
        auto lightMgr = _loc.Get<LightManager>();
        auto atmosphereMgr = _loc.Get<AtmosphereManager>();

        VolumetricLightingUbo ubo{};
        glm::mat4 viewProj = projection * view;
        ubo.invViewProj = glm::inverse(viewProj);
        ubo.prevViewProj = _prevViewProj;

        ubo.gridParams = glm::vec4(_nearPlane, _farPlane * worldScale, 1.0f, (float)kMaxVolumetricCascades);
        ubo.resolution = glm::vec4((float)_gridW, (float)_gridH, (float)_gridD, _intensity);

        // Cascaded Splits
        ubo.cascadeSplits = glm::vec4(50.0f, 150.0f, 500.0f, 2000.0f) * worldScale;

        const auto& lights = lightMgr->GetLights();
        if (!lights.empty()) {
            ubo.sunDir = glm::vec4(-lights[0].direction, lights[0].intensity);
            ubo.sunColor = glm::vec4(lights[0].color, _phaseG);
            if (lights.size() > 1) {
                ubo.moonDir = glm::vec4(-lights[1].direction, lights[1].intensity);
                ubo.moonColor = glm::vec4(lights[1].color, 0.0f);
            }
        }

        const auto& weather = weatherMgr->GetCurrentWeather();
        ubo.hazeParams = glm::vec4(weather.haze_density, weather.haze_height, 0.01f, 0.5f);
        ubo.hazeColor = glm::vec4(weather.haze_color, 1.0f);
        ubo.ambientColor = glm::vec4(atmosphereMgr->GetAmbientEstimate(), _scatteringScale);
        ubo.cloudParams = glm::vec4(weather.cloud_coverage, weather.cloud_density, 0.5f, 0.0f);
        ubo.viewPosVol = glm::vec4(cameraPos, worldScale);

        glm::vec3 front = glm::transpose(view)[2]; // Get forward vector from view matrix
        ubo.viewDirVol = glm::vec4(-front.x, -front.y, -front.z, 0.0f);

        ubo.timeParams = glm::vec4(totalTime, (float)_frameIndex++, (float)_debugMode, 0.0f);

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
            // Shadows UBO layout: mat4[16] (1024) + vec4 (16) + int/padding (16) = 1056 bytes
            glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(), shadowMgr->GetShadowUbo(), 0, 1056);
        }

        // Bind Lighting UBO for viewPos and viewDir
        // (VisualizerImpl usually handles this but we do it here for compute safety)
        // Note: We use the actual buffer handle from the service if needed,
        // but for now we assume it's already bound to LIGHTING_BINDING by VisualizerImpl.

        // Bind SH Probes and Terrain Data
        auto terrainMgr = _loc.Get<TerrainRenderManager>();
        auto atmosphereMgr = _loc.Get<AtmosphereManager>();

        if (terrainMgr) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainProbes(), terrainMgr->GetProbeBuffer());
            terrainMgr->BindTerrainData(*_lightingInjectionShader);
            terrainMgr->BindTerrainData(*_densityVoxelizationShader);
        }

        if (atmosphereMgr) {
            atmosphereMgr->BindToShader(*_lightingInjectionShader);

            // Bind Transmittance LUT - CRITICAL for directional light scattering
            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereTransmittance());
            glBindTexture(GL_TEXTURE_2D, atmosphereMgr->GetTransmittanceLUT());
            _lightingInjectionShader->trySetInt("u_transmittanceLUT", Constants::TextureUnit::AtmosphereTransmittance());

            // Bind Cloud Shadow Map specifically for volumetric injection
            glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereCloudShadow());
            glBindTexture(GL_TEXTURE_2D, atmosphereMgr->GetCloudShadowMap());
            _lightingInjectionShader->trySetInt("u_cloudShadowMap", Constants::TextureUnit::AtmosphereCloudShadow());
        }

        for (int cascade = 0; cascade < kMaxVolumetricCascades; ++cascade) {
            int div = 1 << cascade;
            int cw = _gridW / div;
            int ch = _gridH / div;
            int cd = _gridD;

            _densityVoxelizationShader->use();
            _densityVoxelizationShader->setInt("u_cascadeIndex", cascade);
            if (noiseMgr) noiseMgr->BindDefault(*_densityVoxelizationShader);
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
            if (noiseMgr) noiseMgr->BindDefault(*_lightingInjectionShader);
            // Must use GL_RGBA32F to match the texture format and avoid binding errors
            glBindImageTexture(0, _scatteringVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _densityVolumes[cascade]);
            _lightingInjectionShader->setInt("u_densityVolume", 1);
            if (weatherMgr) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, weatherMgr->GetAerosolTexture());
                _lightingInjectionShader->trySetInt("u_aerosolTexture", 2);
            }
            if (shadowMgr && lightMgr) {
                // We bind to both shadowMaps (Shadow Sampler) and u_shadowMapsRaw (Normal Sampler)
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMaps());
                glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMgr->GetShadowMapArray());
                _lightingInjectionShader->trySetInt("shadowMaps", Constants::TextureUnit::ShadowMaps());

                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::ShadowMapsRaw());
                glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMgr->GetShadowMapArray());
                _lightingInjectionShader->trySetInt("u_shadowMapsRaw", Constants::TextureUnit::ShadowMapsRaw());

                std::array<int, 10> shadow_indices;
                shadow_indices.fill(-1);
                const auto& all_lights = lightMgr->GetLights();
                for (size_t j = 0; j < all_lights.size() && j < 10; ++j) {
                    shadow_indices[j] = all_lights[j].shadow_map_index;
                }
                glUniform1iv(glGetUniformLocation(_lightingInjectionShader->ID, "lightShadowIndices"), 10, shadow_indices.data());
            }
            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, (cd + 7) / 8);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

            _integrationShader->use();
            _integrationShader->setInt("u_cascadeIndex", cascade);
            if (noiseMgr) noiseMgr->BindDefault(*_integrationShader);
            glBindImageTexture(0, _integratedVolumes[cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _scatteringVolumes[cascade]);
            _integrationShader->setInt("u_scatteringVolume", 1);

            if (cascade > 0) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_3D, _integratedVolumes[cascade - 1]);
                _integrationShader->setInt("u_previousIntegrated", 2);
            }

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
