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
        void Update(const glm::vec3& sunDir, const glm::vec3& cameraPos);

        GLuint GetTransmittanceLUT() const { return _transmittanceLUT; }
        GLuint GetMultiScatteringLUT() const { return _multiScatteringLUT; }
        GLuint GetSkyViewLUT() const { return _skyViewLUT; }
        GLuint GetAerialPerspectiveLUT() const { return _aerialPerspectiveLUT; }

        void BindTextures(GLuint firstUnit = 10);

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
    };

} // namespace Boidsish

#endif // ATMOSPHERE_MANAGER_H
