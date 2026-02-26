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
		static void Optimize(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const std::string&         model_name = "unknown"
		);

		/**
		 * @brief Simplifies a mesh to reduce vertex/index count while maintaining appearance.
		 * @param vertices Mesh vertices
		 * @param indices Mesh indices
		 * @param target_error The target error rate (e.g. 0.01 for 1%)
		 * @param target_ratio The target index count ratio (0.0 to 1.0) as a hard limit
		 * @param flags meshopt_Simplify flags for aggression
		 * @param model_name Name of the model for logging
		 */
		static void Simplify(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			float                      target_error,
			float                      target_ratio = 0.5f,
			unsigned int               flags = 0,
			const std::string&         model_name = "unknown"
		);
	};

} // namespace Boidsish
