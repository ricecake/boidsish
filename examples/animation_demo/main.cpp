#include "graphics.h"
#include "model.h"
#include "animator.h"
#include <iostream>

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1280, 720, "Animation Demo");

		auto bird = std::make_shared<Model>("assets/smolbird.fbx");
		bird->SetPosition(0.0f, 2.0f, 0.0f);
		bird->SetScale(glm::vec3(0.01f));
		bird->SetAnimation(0); // Play first animation
		viz.AddShape(bird);

		viz.AddPrepareCallback([](Visualizer& v) {
			v.GetCamera().x = 0.0f;
			v.GetCamera().y = 3.0f;
			v.GetCamera().z = 5.0f;
			v.GetCamera().pitch = -20.0f;
			v.GetCamera().yaw = 0.0f;
		});

		viz.AddShapeHandler([&](float /* time */) {
			bird->UpdateAnimation(viz.GetLastFrameTime());
			return std::vector<std::shared_ptr<Shape>>{};
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
