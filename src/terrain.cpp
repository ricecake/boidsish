#include "terrain.h"

namespace Boidsish {

    Terrain::Terrain(
        const std::vector<unsigned int>& indices,
        const std::vector<glm::vec3>&    vertices,
        const std::vector<glm::vec3>&    normals,
        const PatchProxy&                proxy,
        const glm::vec3&                 position,
        int                              chunk_x,
        int                              chunk_z
    ):
        indices(indices),
        vertices(vertices),
        normals(normals),
        proxy(proxy),
        position(position),
        chunk_x(chunk_x),
        chunk_z(chunk_z)
    {
        // This class is now a plain data container.
    }

    Terrain::~Terrain() = default;

} // namespace Boidsish
