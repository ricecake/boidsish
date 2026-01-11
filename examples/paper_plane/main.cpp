#include <iostream>
#include <memory>
#include <vector>

#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "PaperPlaneInputController.h"
#include "GuidedMissile.h"
#include "GuidedMissileLauncher.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Paper Plane Demo");

		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto plane_id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(plane_id);
		plane->SetPosition(0, height + 10, 0);

		handler.AddEntity<GuidedMissileLauncher>(Boidsish::Vector3(100, height + 10, 0), glm::quat());

		Boidsish::Camera camera(0.0f, height + 15, -10.0f);
		visualizer->SetCamera(camera);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(plane);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
