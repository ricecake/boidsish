#include "decor_manager.h"
#include "graphics.h"

using namespace Boidsish;

int main() {
	Visualizer vis(1280, 720, "Foliage Demo");

	auto decor = vis.GetDecorManager();
	if (decor) {
		// Add some decor types
		// Note: Using existing models as placeholders
		decor->AddDecorType("assets/cube.obj", 0.5f);         // Grass-like density
		decor->AddDecorType("assets/utah_teapot.obj", 0.05f); // Sparse trees/rocks
	}

	// Set camera to a good starting position
	Camera cam = vis.GetCamera();
	cam.y = 50.0f;
	cam.pitch = -30.0f;
	vis.SetCamera(cam);

	vis.Run();

	return 0;
}
