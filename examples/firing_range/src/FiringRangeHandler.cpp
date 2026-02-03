#include "FiringRangeHandler.h"
#include "GuidedMissileLauncher.h"
#include "CatMissile.h"
#include "PaperPlane.h"

namespace Boidsish {
	FiringRangeHandler::FiringRangeHandler(task_thread_pool::task_thread_pool& thread_pool)
		: PaperPlaneHandler(thread_pool) {
		SetAutoSpawn(false);
	}

	void FiringRangeHandler::PreTimestep(float time, float delta_time) {
		PaperPlaneHandler::PreTimestep(time, delta_time);

		if (auto_fire) {
			last_fire_time += delta_time;
			if (last_fire_time >= fire_interval) {
				last_fire_time = 0.0f;
				if (auto_fire_type == MissileType::Guided) {
					auto launchers = GetEntitiesByType<GuidedMissileLauncher>();
					for (auto launcher : launchers) {
						launcher->Fire(*this);
					}
				} else if (auto_fire_type == MissileType::Cat) {
					auto launchers = GetEntitiesByType<GuidedMissileLauncher>();
					for (auto launcher : launchers) {
						// Spawn a CatMissile from the launcher's position
						QueueAddEntity<CatMissile>(
							launcher->GetPosition(),
							launcher->GetOrientation(),
							glm::vec3(0, 0, -1), // eject direction
							Vector3(0, 0, 0)     // initial velocity
						);
					}
				}
			}
		}
	}
}
