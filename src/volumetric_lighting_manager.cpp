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
        for (int f = 0; f < 3; ++f) {
            for (int i = 0; i < kMaxVolumetricCascades; ++i) {
                _densityVolumes[f][i] = 0;
                _scatteringVolumes[f][i] = 0;
                _integratedVolumes[f][i] = 0;
            }
        }
    }

    VolumetricLightingManager::~VolumetricLightingManager() {
        for (int f = 0; f < 3; ++f) {
            for (int i = 0; i < kMaxVolumetricCascades; ++i) {
                if (_densityVolumes[f][i]) glDeleteTextures(1, &_densityVolumes[f][i]);
                if (_scatteringVolumes[f][i]) glDeleteTextures(1, &_scatteringVolumes[f][i]);
                if (_integratedVolumes[f][i]) glDeleteTextures(1, &_integratedVolumes[f][i]);
            }
        }
        _parameterUbo.reset();
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

        for (int f = 0; f < 3; ++f) {
            for (int i = 0; i < kMaxVolumetricCascades; ++i) {
                // Distant cascades can have lower resolution
                int div = 1 << i;
                // Density volumes MUST use NEAREST filtering for integer textures
                create3DTexture(_densityVolumes[f][i], GL_R32UI, _gridW / div, _gridH / div, _gridD, GL_RED_INTEGER, GL_UNSIGNED_INT);
                glBindTexture(GL_TEXTURE_3D, _densityVolumes[f][i]);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                // Switch to RGBA16F for integration volumes to reduce memory footprint
                create3DTexture(_scatteringVolumes[f][i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
                create3DTexture(_integratedVolumes[f][i], GL_RGBA16F, _gridW / div, _gridH / div, _gridD);
            }
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
        _parameterUbo = std::make_unique<PersistentBuffer<VolumetricLightingUbo>>(GL_UNIFORM_BUFFER, 1, 3);
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

        if (_parameterUbo) {
            *(_parameterUbo->GetFrameDataPtr()) = ubo;
        }

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
        if (_parameterUbo) {
            glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VolumetricLighting(),
                _parameterUbo->GetBufferId(), _parameterUbo->GetFrameOffset(), sizeof(VolumetricLightingUbo));
        }

        // Bind WindData UBO for world-space aerosol mapping

        // Bind Shadows UBO for CSM
        if (shadowMgr) {
            glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Shadows(),
                shadowMgr->GetShadowUbo(), shadowMgr->GetShadowUboOffset(), sizeof(ShadowUbo));
        }

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
            glBindImageTexture(0, _densityVolumes[_textureFrameIndex][cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32UI);
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
                glBindImageTexture(0, _densityVolumes[_textureFrameIndex][cascade], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

                // Particle buffer is already bound to PARTICLE_BUFFER_BINDING in VisualizerImpl::UpdateSystems

                glDispatchCompute((Constants::Class::Particles::MaxParticles() + 255) / 256, 1, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }

            _lightingInjectionShader->use();
            _lightingInjectionShader->setInt("u_cascadeIndex", cascade);
            if (noiseMgr) noiseMgr->BindDefault(*_lightingInjectionShader);
            // Must use GL_RGBA16F to match the texture format
            glBindImageTexture(0, _scatteringVolumes[_textureFrameIndex][cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _densityVolumes[_textureFrameIndex][cascade]);
            _lightingInjectionShader->setInt("u_densityVolume", 1);
            if (weatherMgr) {
                glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AerosolData());
                glBindTexture(GL_TEXTURE_2D, weatherMgr->GetAerosolTexture());
                _lightingInjectionShader->trySetInt("u_aerosolTexture", Constants::TextureUnit::AerosolData());
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
            glBindImageTexture(0, _integratedVolumes[_textureFrameIndex][cascade], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, _scatteringVolumes[_textureFrameIndex][cascade]);
            _integrationShader->setInt("u_scatteringVolume", 1);

            if (cascade > 0) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_3D, _integratedVolumes[_textureFrameIndex][cascade - 1]);
                _integrationShader->setInt("u_previousIntegrated", 2);
            }

            glDispatchCompute((cw + 7) / 8, (ch + 7) / 8, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    void VolumetricLightingManager::BindToShader(class ShaderBase& shader) {
        // Read from the previous frame's completed volume to avoid race conditions/strobing
        int readIdx = (_textureFrameIndex + 2) % 3;

        for (int i = 0; i < kMaxVolumetricCascades; ++i) {
            std::string name = "u_volumetricIntegrated[" + std::to_string(i) + "]";
            int unit = Constants::TextureUnit::VolumetricCascade0() + i;
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_3D, _integratedVolumes[readIdx][i]);
            shader.trySetInt(name, unit);
        }

        // Also bind the VolumetricLighting UBO
        GLuint ubo_idx = glGetUniformBlockIndex(shader.ID, "VolumetricLighting");
        if (ubo_idx != GL_INVALID_INDEX) {
            glUniformBlockBinding(shader.ID, ubo_idx, Constants::UboBinding::VolumetricLighting());
        }
    }

    GLuint VolumetricLightingManager::GetIntegratedVolume() const {
        int readIdx = (_textureFrameIndex + 2) % 3;
        return _integratedVolumes[readIdx][0]; // Return near cascade by default
    }

} // namespace Boidsish
