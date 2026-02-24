#include "neighbor_utils.h"

#include <cmath>
#include <memory>
#include <vector>

#include "terrain.h"

namespace Boidsish {

	std::vector<const Terrain*>
	get_neighbors(const Terrain* chunk, const std::vector<std::shared_ptr<Terrain>>& all_chunks) {
		std::vector<const Terrain*> neighbors;
		int                         center_x = chunk->GetX();
		int                         center_z = chunk->GetZ();
		float                       chunk_size = std::sqrt(chunk->proxy.radiusSq) * 2.0f;

		for (const auto& other_chunk_ptr : all_chunks) {
			const Terrain* other_chunk = other_chunk_ptr.get();
			if (other_chunk == chunk) {
				continue;
			}

			int other_x = other_chunk->GetX();
			int other_z = other_chunk->GetZ();

			bool is_neighbor = std::abs(other_x - center_x) <= chunk_size && std::abs(other_z - center_z) <= chunk_size;

			if (is_neighbor) {
				neighbors.push_back(other_chunk);
			}
		}

		return neighbors;
	}

} // namespace Boidsish
