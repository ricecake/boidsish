#ifndef CHECKPOINT_SPLINE_H
#define CHECKPOINT_SPLINE_H

#include "path.h"
#include <memory>
#include <vector>

namespace Boidsish {

	class ITerrainGenerator;
	class EntityHandler;

	class CheckpointSpline: public Path {
	public:
		CheckpointSpline(int id = 0);

		/**
		 * @brief Syncs waypoints with a list of checkpoint entities.
		 */
		void UpdateFromCheckpoints(const std::vector<int>& ids, const EntityHandler& handler);

		/**
		 * @brief Ensures the spline doesn't intersect terrain by adding intermediate waypoints.
		 */
		void SubdivideAndAdjustForTerrain(std::shared_ptr<ITerrainGenerator> terrain, float clearance = 20.0f);

	private:
		void GetControlPoints(int segmentIndex, Vector3& p0, Vector3& p1, Vector3& p2, Vector3& p3) const;
	};

} // namespace Boidsish

#endif // CHECKPOINT_SPLINE_H
