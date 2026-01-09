#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include "bomb.h"
#include "dot.h"
#include "emplacements.h"
#include "graphics.h"
#include "guided_missile.h"
#include "handler.h"
#include "hud.h"
#include "logger.h"
#include "model.h"
#include "plane.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <fire_effect.h>
#include <GLFW/glfw3.h>

using namespace Boidsish;

// class CatMissile;
// class CatBomb;
// class GuidedMissile;
// class PaperPlane;
// class GuidedMissileLauncher;

// static int selected_weapon = 0;

// class MakeBranchAttractor {
// private:
// 	std::random_device                    rd;
// 	std::mt19937                          eng;
// 	std::uniform_real_distribution<float> x;
// 	std::uniform_real_distribution<float> y;
// 	std::uniform_real_distribution<float> z;

// public:
// 	MakeBranchAttractor(): eng(rd()), x(-1, 1), y(0, 1), z(-1, 1) {}

// 	Vector3 operator()(float r) { return r * Vector3(x(eng), y(eng), z(eng)).Normalized(); }
// };

// static auto missilePicker = MakeBranchAttractor();

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Terrain Demo");
		visualizer->AddHudIcon({1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, true});
		visualizer->AddHudIcon({2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, false});

		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);
		plane->SetPosition(0, height + 10, 0);
		Boidsish::Camera camera(0.0f, height + 15, -10.0f);
		visualizer->SetCamera(camera);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(plane);

		visualizer->AddHudGauge({3, 100.0f, "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

		auto controller = std::make_shared<PaperPlaneInputController>();
		std::dynamic_pointer_cast<PaperPlane>(plane)->SetController(controller);

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			controller->pitch_up = state.keys[GLFW_KEY_S];
			controller->pitch_down = state.keys[GLFW_KEY_W];
			controller->yaw_left = state.keys[GLFW_KEY_A];
			controller->yaw_right = state.keys[GLFW_KEY_D];
			controller->roll_left = state.keys[GLFW_KEY_Q];
			controller->roll_right = state.keys[GLFW_KEY_E];
			controller->boost = state.keys[GLFW_KEY_LEFT_SHIFT];
			controller->brake = state.keys[GLFW_KEY_LEFT_CONTROL];
			controller->fire = state.keys[GLFW_KEY_SPACE];
			if (state.key_down[GLFW_KEY_F]) {
				controller->selected_weapon = (controller->selected_weapon + 1) % 2;
				visualizer->UpdateHudIcon(
					1,
					{1,
				     "assets/missile-icon.png",
				     HudAlignment::TOP_LEFT,
				     {10, 10},
				     {64, 64},
				     controller->selected_weapon == 0}
				);
				visualizer->UpdateHudIcon(
					2,
					{2,
				     "assets/bomb-icon.png",
				     HudAlignment::TOP_LEFT,
				     {84, 10},
				     {64, 64},
				     controller->selected_weapon == 1}
				);
			}
		});

		std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
		visualizer->AddShapeHandler([&](float time) { return shapes; });
		auto model = std::make_shared<Boidsish::Model>("assets/utah_teapot.obj");
		model->SetColossal(true);
		shapes.push_back(model);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
