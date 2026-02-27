#pragma once

#include "procedural_ir.h"
#include "model.h"

namespace Boidsish {

    class ProceduralMesher {
    public:
        static std::shared_ptr<Model> GenerateModel(const ProceduralIR& ir);

    private:
        struct SDFConfig {
            float grid_size = 0.05f;
            float smooth_k = 0.1f;
            glm::vec3 bounds_min;
            glm::vec3 bounds_max;
        };

        static float SampleSDF(const glm::vec3& p, const ProceduralIR& ir, const SDFConfig& config, glm::vec3& out_color);
        static std::shared_ptr<ModelData> GenerateSurfaceNets(const ProceduralIR& ir, const SDFConfig& config);
    };

} // namespace Boidsish
