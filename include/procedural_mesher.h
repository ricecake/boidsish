#pragma once

#include "procedural_ir.h"
#include "model.h"

namespace Boidsish {

    class ProceduralMesher {
    public:
        static std::shared_ptr<Model> GenerateModel(const ProceduralIR& ir);

    private:
        static std::shared_ptr<ModelData> GenerateDirectMesh(const ProceduralIR& ir);
    };

} // namespace Boidsish
