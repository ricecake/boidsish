#include "decor_manager.h"
#include "graphics.h"

using namespace Boidsish;

int main() {
	Visualizer vis(1280, 720, "Foliage Demo");

	auto decor = vis.GetDecorManager();
	if (decor) {
		// Add some decor types with custom properties

		// Teapots - aligned to terrain normal (like bushes on a cliff)
		DecorProperties teapot_props;
		teapot_props.density = 0.1f;
		teapot_props.base_scale = 0.5f;
		teapot_props.scale_variance = 0.1f;
		teapot_props.align_to_terrain = true; // Align to slope
		decor->AddDecorType("assets/utah_teapot.obj", teapot_props);

		// Missiles - always point up regardless of terrain (like trees)
		DecorProperties missile_props;
		missile_props.density = 0.02f;
		missile_props.base_scale = 2.0f;
		missile_props.scale_variance = 0.5f;
		missile_props.base_rotation = glm::vec3(-90.0f, 0.0f, 0.0f); // Point upward (rotate -90 on X)
		missile_props.random_yaw = true;
		missile_props.align_to_terrain = false; // Stay upright (default)
		decor->AddDecorType("assets/Missile.obj", missile_props);
	}

	// Set camera to a good starting position
	Camera cam = vis.GetCamera();
	cam.y = 50.0f;
	cam.pitch = -30.0f;
	vis.SetCamera(cam);

	vis.Run();

	return 0;
}
