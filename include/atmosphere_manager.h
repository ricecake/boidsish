#ifndef ATMOSPHERE_MANAGER_H
#define ATMOSPHERE_MANAGER_H

#include <memory>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

class ComputeShader;

namespace Boidsish {

    class AtmosphereManager {
    public:
        AtmosphereManager();
        ~AtmosphereManager();

        void Initialize();
        void Update(const glm::vec3& sunDir, const glm::vec3& sunColor, float sunIntensity, const glm::vec3& cameraPos);

        glm::vec3 GetAmbientEstimate() const { return _ambientEstimate; }

        GLuint GetTransmittanceLUT() const { return _transmittanceLUT; }
        GLuint GetMultiScatteringLUT() const { return _multiScatteringLUT; }
        GLuint GetSkyViewLUT() const { return _skyViewLUT; }
        GLuint GetAerialPerspectiveLUT() const { return _aerialPerspectiveLUT; }

        void BindTextures(GLuint firstUnit = 10);

        // Parameters
        void SetRayleighScale(float s) {
            if (s != _rayleighScale) { _rayleighScale = s; _needsPrecompute = true; }
        }
        float GetRayleighScale() const { return _rayleighScale; }
        void SetMieScale(float s) {
            if (s != _mieScale) { _mieScale = s; _needsPrecompute = true; }
        }
        float GetMieScale() const { return _mieScale; }
        void SetMieAnisotropy(float g) {
            if (g != _mieAnisotropy) { _mieAnisotropy = g; _needsPrecompute = true; }
        }
        float GetMieAnisotropy() const { return _mieAnisotropy; }
        void SetMultiScatteringScale(float s) { _multiScatScale = s; }
        float GetMultiScatteringScale() const { return _multiScatScale; }
        void SetAmbientScatteringScale(float s) { _ambientScatScale = s; }
        float GetAmbientScatteringScale() const { return _ambientScatScale; }

    private:
        void CreateTextures();
        void CreateShaders();

        GLuint _transmittanceLUT = 0;
        GLuint _multiScatteringLUT = 0;
        GLuint _skyViewLUT = 0;
        GLuint _aerialPerspectiveLUT = 0;

        std::unique_ptr<ComputeShader> _transmittanceShader;
        std::unique_ptr<ComputeShader> _multiScatteringShader;
        std::unique_ptr<ComputeShader> _skyViewShader;
        std::unique_ptr<ComputeShader> _aerialPerspectiveShader;

        bool _needsPrecompute = true;

        float _rayleighScale = 1.0f;
        float _mieScale = 0.1f;
        float _mieAnisotropy = 0.8f;
        float _multiScatScale = 0.1f;
        float _ambientScatScale = 0.1f;

        glm::vec3 _ambientEstimate = glm::vec3(0.0f);
    };

} // namespace Boidsish

#endif // ATMOSPHERE_MANAGER_H
