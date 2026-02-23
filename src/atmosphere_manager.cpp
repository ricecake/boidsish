#include "atmosphere_manager.h"
#include "shader.h"
#include <iostream>

namespace Boidsish {

    AtmosphereManager::AtmosphereManager() {}

    AtmosphereManager::~AtmosphereManager() {
        if (_transmittanceLUT) glDeleteTextures(1, &_transmittanceLUT);
        if (_multiScatteringLUT) glDeleteTextures(1, &_multiScatteringLUT);
        if (_skyViewLUT) glDeleteTextures(1, &_skyViewLUT);
        if (_aerialPerspectiveLUT) glDeleteTextures(1, &_aerialPerspectiveLUT);
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
    }

    void AtmosphereManager::CreateShaders() {
        _transmittanceShader = std::make_unique<ComputeShader>("shaders/atmosphere/transmittance_lut.comp");
        _multiScatteringShader = std::make_unique<ComputeShader>("shaders/atmosphere/multiscattering_lut.comp");
        _skyViewShader = std::make_unique<ComputeShader>("shaders/atmosphere/sky_view_lut.comp");
        _aerialPerspectiveShader = std::make_unique<ComputeShader>("shaders/atmosphere/aerial_perspective_lut.comp");
    }

    void AtmosphereManager::Update(
        const glm::vec3& sunDir,
        const glm::vec3& sunColor,
        float            sunIntensity,
        const glm::vec3& cameraPos
    ) {
        if (_needsPrecompute) {
            // Dispatch Transmittance
            _transmittanceShader->use();
            _transmittanceShader->setFloat("u_rayleighScale", _rayleighScale);
            _transmittanceShader->setFloat("u_mieScale", _mieScale);
            glBindImageTexture(0, _transmittanceLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            glDispatchCompute(256 / 8, 64 / 8, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // Dispatch MultiScattering
            _multiScatteringShader->use();
            _multiScatteringShader->setFloat("u_rayleighScale", _rayleighScale);
            _multiScatteringShader->setFloat("u_mieScale", _mieScale);
            _multiScatteringShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
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
        _skyViewShader->setFloat("u_rayleighScale", _rayleighScale);
        _skyViewShader->setFloat("u_mieScale", _mieScale);
        _skyViewShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
        _skyViewShader->setFloat("u_multiScatScale", _multiScatScale);
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
        _aerialPerspectiveShader->setFloat("u_rayleighScale", _rayleighScale);
        _aerialPerspectiveShader->setFloat("u_mieScale", _mieScale);
        _aerialPerspectiveShader->setFloat("u_mieAnisotropy", _mieAnisotropy);
        _aerialPerspectiveShader->setFloat("u_multiScatScale", _multiScatScale);
        _aerialPerspectiveShader->setFloat("u_ambientScatScale", _ambientScatScale);
        glDispatchCompute(32 / 4, 32 / 4, 32 / 4);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

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
