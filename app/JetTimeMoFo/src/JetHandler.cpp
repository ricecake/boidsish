#include "JetHandler.h"
#include "graphics.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	JetHandler::JetHandler(task_thread_pool::task_thread_pool& thread_pool) :
		SpatialEntityHandler(thread_pool) {
	}

	void JetHandler::PreparePlane(std::shared_ptr<JetPlane> plane) {
		if (!plane || !vis || !vis->GetTerrain())
			return;

		glm::vec3 start_pos(0, 100, 0);
		auto [h, norm] = vis->GetTerrainPropertiesAtPoint(start_pos.x, start_pos.z);
		start_pos.y = h + 50.0f;

		plane->SetPosition(start_pos.x, start_pos.y, start_pos.z);
		plane->SetVelocity(Vector3(0, 0, -30.0f));
		plane->UpdateShape();

		vis->GetCamera().x = start_pos.x;
		vis->GetCamera().y = start_pos.y;
		vis->GetCamera().z = start_pos.z;
	}

} // namespace Boidsish
