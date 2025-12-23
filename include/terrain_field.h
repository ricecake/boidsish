#pragma once

#include "field.h"
#include "terrain.h"
#include "vector.h"
#include <vector>

namespace Boidsish {

class TerrainField {
public:
    TerrainField(const std::vector<std::shared_ptr<Terrain>>& terrains, float influence_radius);
    Vector3 getInfluence(const Vector3& position) const;

private:
    struct Patch {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        PatchProxy proxy;
    };

    std::vector<Patch> patches_;
    WendlandLUT lut_;
};

}
