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
        glm::vec4 hazeParams;     // x: haze density, y: haze height, z: noise scale, w: noise strength
        glm::vec4 ambientColor;   // rgb: color, w: scattering scale
        glm::vec4 cloudParams;    // x: cloud coverage, y: cloud density, z: cloud shadow intensity, w: reserved
        glm::vec4 cascadeSplits;  // Near plane distances for each cascade
    };

    class VolumetricLightingManager : public IManager {
    public:
        VolumetricLightingManager(ServiceLocator& loc);
        ~VolumetricLightingManager();

        void Initialize() override;
        void Update(float deltaTime, float totalTime, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

        void Dispatch(
            const glm::mat4& view,
            const glm::mat4& projection,
            const glm::vec3& cameraPos,
            float            totalTime
        );

        GLuint GetIntegratedVolume() const;

        void BindToShader(class ShaderBase& shader);

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
        std::unique_ptr<ComputeShader> _gridInitShader;
        std::unique_ptr<ComputeShader> _densityVoxelizationShader;
        std::unique_ptr<ComputeShader> _lightingInjectionShader;
        std::unique_ptr<ComputeShader> _integrationShader;

        // Grid configuration
        int _gridW = 160;
        int _gridH = 90;
        int _gridD = 64;

        float _nearPlane = 0.1f;
        float _farPlane = 100.0f; // Standard volumetric range

        glm::mat4 _prevViewProj{1.0f};
    };

} // namespace Boidsish
