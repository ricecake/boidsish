#ifndef TEMPORAL_DATA_H
#define TEMPORAL_DATA_H

#include <glm/glm.hpp>

namespace Boidsish {
	/**
	 * @brief GPU-compatible temporal data for UBO upload (std140 layout).
	 * Used for temporal reprojection, GTAO, and other temporal effects.
	 */
	struct alignas(16) TemporalUbo {
		glm::mat4 viewProjection;     // offset 0, 64 bytes
		glm::mat4 prevViewProjection; // offset 64, 64 bytes
		glm::mat4 uProjection;        // offset 128, 64 bytes
		glm::mat4 invProjection;      // offset 192, 64 bytes
		glm::mat4 invView;            // offset 256, 64 bytes
		glm::vec2 texelSize;          // offset 320, 8 bytes
		int       frameIndex;         // offset 328, 4 bytes
		float     padding;            // offset 332, 4 bytes
	}; // Total: 336 bytes

	static_assert(sizeof(TemporalUbo) == 336, "TemporalUbo size mismatch for std140");
} // namespace Boidsish

#endif // TEMPORAL_DATA_H
