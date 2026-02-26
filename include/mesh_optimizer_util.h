#pragma once
#include <vector>
#include "geometry.h"

namespace Boidsish {

	class MeshOptimizerUtil {
	public:
		/**
		 * @brief Optimizes a mesh for better performance on the GPU.
		 * Performs vertex cache, vertex fetch, and overdraw optimization.
		 */
		static void Optimize(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);

		/**
		 * @brief Simplifies a mesh to reduce vertex/index count while maintaining appearance.
		 * @param vertices Mesh vertices
		 * @param indices Mesh indices
		 * @param target_ratio The target index count ratio (0.0 to 1.0)
		 */
		static void Simplify(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, float target_ratio);
	};

} // namespace Boidsish
