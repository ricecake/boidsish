#pragma once

#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"
#include "shader.h"

namespace Boidsish {

    class ServiceLocator;
    class WeatherManager;
    class FireEffectManager;

    static constexpr int kMaxVolumetricCascades = 4;

    struct VolumetricLightingUbo {
        glm::mat4 invViewProj;
        glm::mat4 prevViewProj;
        glm::vec4 gridParams;     // x: near, y: far, z: log bias, w: cascade count
        glm::vec4 resolution;     // x: gridW, y: gridH, z: gridD, w: intensity
        glm::vec4 sunDir;         // xyz: dir, w: sun intensity
        glm::vec4 sunColor;       // rgb: color, w: mie anisotropy (g)
        glm::vec4 moonDir;        // xyz: dir, w: moon intensity
        glm::vec4 moonColor;      // rgb: color, w: reserved
        glm::vec4 hazeParams;     // x: haze density, y: haze height, z: noise scale, w: noise strength
        glm::vec4 hazeColor;      // rgb: color, w: reserved
        glm::vec4 ambientColor;   // rgb: color, w: scattering scale
        glm::vec4 cloudParams;    // x: cloud coverage, y: cloud density, z: cloud shadow intensity, w: reserved
        glm::vec4 cascadeSplits;  // Near plane distances for each cascade
        glm::vec4 viewPosVol;     // xyz: camera world pos, w: world scale
        glm::vec4 viewDirVol;     // xyz: camera forward, w: reserved
        glm::vec4 timeParams;     // x: totalTime, y: frameIndex, z: debugMode, w: reserved
    };

    class VolumetricLightingManager : public IManager {
    public:
        VolumetricLightingManager(ServiceLocator& loc);
        ~VolumetricLightingManager();

        void Initialize() override;
        void Update(float deltaTime, float totalTime, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float worldScale);

        void Dispatch(
            const glm::mat4& view,
            const glm::mat4& projection,
            const glm::vec3& cameraPos,
            float            totalTime
        );

        GLuint GetIntegratedVolume() const;

        void BindToShader(class ShaderBase& shader);

        // Parameters
        float GetIntensity() const { return _intensity; }
        void  SetIntensity(float i) { _intensity = i; }
        float GetScatteringScale() const { return _scatteringScale; }
        void  SetScatteringScale(float s) { _scatteringScale = s; }
        float GetExtinctionScale() const { return _extinctionScale; }
        void  SetExtinctionScale(float e) { _extinctionScale = e; }
        float GetPhaseG() const { return _phaseG; }
        void  SetPhaseG(float g) { _phaseG = g; }
        int   GetDebugMode() const { return _debugMode; }
        void  SetDebugMode(int m) { _debugMode = m; }

    private:
        void CreateTextures();
        void CreateShaders();
        void CreateBuffers();

        ServiceLocator& _loc;

        // 3D textures for the froxel grid (one set per cascade)
        GLuint _densityVolumes[kMaxVolumetricCascades];
        GLuint _scatteringVolumes[kMaxVolumetricCascades];
        GLuint _integratedVolumes[kMaxVolumetricCascades];

        // UBO for parameters
        GLuint _parameterUbo = 0;

        // Compute shaders
        std::unique_ptr<ComputeShader> _densityVoxelizationShader;
        std::unique_ptr<ComputeShader> _particleVoxelizationShader;
        std::unique_ptr<ComputeShader> _lightingInjectionShader;
        std::unique_ptr<ComputeShader> _integrationShader;

        // Grid configuration
        int _gridW = 200;
        int _gridH = 112;
        int _gridD = 200; // Further increased depth for better god ray resolution

        float _nearPlane = 1.0f;
        float _farPlane = 2000.0f; // Extended volumetric range
        int   _frameIndex = 0;

        // Runtime params
        float _intensity = 80.0f;
        float _scatteringScale = 60.0f;
        float _extinctionScale = 0.2f;
        float _phaseG = 0.95f;
        int   _debugMode = 0;

        glm::mat4 _prevViewProj{1.0f};
    };

} // namespace Boidsish
