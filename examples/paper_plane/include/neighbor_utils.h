#pragma once

#include <vector>
#include <memory>

namespace Boidsish {

class Terrain; // Forward declaration

std::vector<const Terrain*>
get_neighbors(const Terrain* chunk, const std::vector<std::shared_ptr<Terrain>>& all_chunks);

} // namespace Boidsish
