#include <iostream>
#include <memory>
#include <vector>

#include "PaperPlane.h"
#include "TargetPlane.h"
#include "FiringRangeHandler.h"
#include "FiringRangeWidget.h"
#include "constants.h"
#include "decor_manager.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include <GLFW/glfw3.h>
#include "GuidedMissileLauncher.h"

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(
			Constants::Project::Window::DefaultWidth(),
			Constants::Project::Window::DefaultHeight(),
			"Missile Firing Range"
		);

		auto decor = visualizer->GetDecorManager();

		DecorProperties teapot_props;
		teapot_props.min_height = 0.01;
		teapot_props.min_density = 0.1f;
		teapot_props.max_density = 0.3f;
		teapot_props.base_scale = 0.005f;
		teapot_props.scale_variance = 0.001f;
		teapot_props.align_to_terrain = true;
		decor->AddDecorType("assets/tree01.obj", teapot_props);

		auto handler = std::make_shared<FiringRangeHandler>(visualizer->GetThreadPool());
		handler->SetVisualizer(visualizer);

		// Initial setup: A target and some launchers
		handler->AddEntity<TargetPlane>(Vector3(0, 100, 0));
		handler->AddEntity<GuidedMissileLauncher>(Vector3(100, 40, 100), glm::quat(1,0,0,0));
		handler->AddEntity<GuidedMissileLauncher>(Vector3(-100, 40, 100), glm::quat(1,0,0,0));
		handler->AddEntity<GuidedMissileLauncher>(Vector3(0, 40, -100), glm::quat(1,0,0,0));

		Boidsish::Camera camera(0.0f, 150.0f, -200.0f, -30.0f, 0.0f);
		visualizer->SetCamera(camera);

		visualizer->AddShapeHandler([h = handler](float time) { return (*h)(time); });

		auto widget = std::make_shared<UI::FiringRangeWidget>(*handler);
		visualizer->AddWidget(widget);

		visualizer->GetAudioManager().PlayMusic("assets/kazoom.mp3", true, 0.25f);
		visualizer->ToggleMenus();

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
