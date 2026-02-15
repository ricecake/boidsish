#include "checkpoint_spline.h"

#include "entity.h"
#include "spline.h"
#include "terrain_generator_interface.h"

namespace Boidsish {

	CheckpointSpline::CheckpointSpline(int id): Path(id) {
		SetVisible(true);
	}

	void CheckpointSpline::UpdateFromCheckpoints(const std::vector<int>& ids, const EntityHandler& handler) {
		ClearWaypoints();
		if (ids.empty())
			return;

		for (int id : ids) {
			auto e = handler.GetEntity(id);
			if (e) {
				glm::vec3 up = e->ObjectToWorld(glm::vec3(0, 1, 0));
				AddWaypoint(e->GetPosition(), Vector3(up.x, up.y, up.z), 50.0f); // Default size for visibility
			}
		}
	}

	void CheckpointSpline::SubdivideAndAdjustForTerrain(std::shared_ptr<ITerrainGenerator> terrain, float clearance) {
		if (GetWaypoints().size() < 2 || !terrain)
			return;

		int  iterations = 0;
		bool changed = true;
		while (changed && iterations < 3) {
			changed = false;
			iterations++;

			auto&                 waypoints = GetWaypoints();
			std::vector<Waypoint> newWaypoints;
			int                   numSegments = (GetMode() == PathMode::LOOP) ? waypoints.size() : waypoints.size() - 1;

			for (int i = 0; i < numSegments; ++i) {
				newWaypoints.push_back(waypoints[i]);

				// Sample segment
				const int samples = 8;
				float     maxPenetration = 0.0f;
				float     bestT = -1.0f;
				Vector3   bestPos;

				Vector3 p0, p1, p2, p3;
				GetControlPoints(i, p0, p1, p2, p3);

				for (int j = 1; j < samples; ++j) {
					float   t = (float)j / samples;
					Vector3 p = Spline::CatmullRom(t, p0, p1, p2, p3);
					float   distAbove = terrain->GetDistanceAboveTerrain(p.Toglm());
					if (distAbove < clearance) {
						float penetration = clearance - distAbove;
						if (penetration > maxPenetration) {
							maxPenetration = penetration;
							bestT = t;
							bestPos = p;
							float h = std::get<0>(terrain->GetTerrainPropertiesAtPoint(p.x, p.z));
							bestPos.y = h + clearance;
						}
					}
				}

				if (bestT > 0) {
					Waypoint nw = waypoints[i];
					nw.position = bestPos;
					newWaypoints.push_back(nw);
					changed = true;
				}
			}
			if (GetMode() != PathMode::LOOP) {
				newWaypoints.push_back(waypoints.back());
			}

			if (changed) {
				SetWaypoints(newWaypoints);
			}
		}
	}

	void CheckpointSpline::GetControlPoints(int segmentIndex, Vector3& p0, Vector3& p1, Vector3& p2, Vector3& p3) const {
		const auto& waypoints = GetWaypoints();
		int         num_waypoints = waypoints.size();
		if (GetMode() == PathMode::LOOP) {
			p0 = waypoints[(segmentIndex - 1 + num_waypoints) % num_waypoints].position;
			p1 = waypoints[segmentIndex].position;
			p2 = waypoints[(segmentIndex + 1) % num_waypoints].position;
			p3 = waypoints[(segmentIndex + 2) % num_waypoints].position;
		} else {
			p1 = waypoints[segmentIndex].position;
			p2 = waypoints[segmentIndex + 1].position;
			p0 = (segmentIndex > 0) ? waypoints[segmentIndex - 1].position : (p1 - (p2 - p1));
			p3 = (segmentIndex < (int)waypoints.size() - 2) ? waypoints[segmentIndex + 2].position : (p2 + (p2 - p1));
		}
	}

} // namespace Boidsish
