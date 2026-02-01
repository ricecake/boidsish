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
		teapot_props.min_density = 0.01f;
		teapot_props.max_density = 0.03f;
		teapot_props.base_scale = 0.005f;
		teapot_props.scale_variance = 0.001f;
		teapot_props.align_to_terrain = true; // Align to slope
		decor->AddDecorType("assets/tree01.obj", teapot_props);

		// Missiles - always point up regardless of terrain (like trees)
		DecorProperties missile_props;
		missile_props.min_density = 0.01f;
		missile_props.max_density = 0.02f;
		missile_props.base_scale = 0.001f;
		missile_props.scale_variance = 0.005f;
		missile_props.base_rotation = glm::vec3(-90.0f, 0.0f, 0.0f); // Point upward (rotate -90 on X)
		missile_props.random_yaw = true;
		missile_props.align_to_terrain = false; // Stay upright (default)
		decor->AddDecorType("assets/PUSHILIN_dead_tree.obj", missile_props);
	}

	// Set camera to a good starting position
	Camera cam = vis.GetCamera();
	cam.y = 50.0f;
	cam.pitch = -30.0f;
	vis.SetCamera(cam);

	while (!vis.ShouldClose()) {
		vis.Update();
		vis.Render();
	}

	return 0;
}
