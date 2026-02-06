#include <iostream>
#include <memory>
#include <vector>

#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "PaperPlaneInputController.h"
#include "constants.h"
#include "decor_manager.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(
			Constants::Project::Window::DefaultWidth(),
			Constants::Project::Window::DefaultHeight(),
			"Paper Plane Demo"
		);

		auto terrain = visualizer->GetTerrain();
		terrain->SetWorldScale(2.0f);

		auto decor = visualizer->GetDecorManager();

		DecorProperties teapot_props;
		teapot_props.min_height = 0.01;
		teapot_props.max_height = 95.0f;
		teapot_props.min_density = 0.01f;
		teapot_props.max_density = 0.03f;
		teapot_props.base_scale = 0.005f;
		teapot_props.scale_variance = 0.001f;
		teapot_props.align_to_terrain = true; // Align to slope
		decor->AddDecorType("assets/tree01.obj", teapot_props);

		/*
		Want to admust the foliage so that it picks a density between min and max for each chunk as the target density.
		Then it should distribute items such that each one gets placed correctly.
		Need to consider it from the compute angle though.
		Need to use a deterministic function to figure out the weight for each chunk.
		Then need to use a deterministic function to figure out where each item goes.
		Similar to the fire system balancing, but everything has a uniform density...density.
		*/

		visualizer->AddHudIcon(
			{1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, selected_weapon == 0}
		);
		visualizer->AddHudIcon(
			{2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, selected_weapon == 1}
		);
		visualizer->AddHudIcon(
			{3, "assets/bullet-icon.png", HudAlignment::TOP_LEFT, {84 + 10 + 64, 10}, {64, 64}, selected_weapon == 2}
		);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = std::dynamic_pointer_cast<PaperPlane>(handler.GetEntity(id));

		// Find a good starting position and orientation
		handler.PreparePlane(plane);

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
			controller->chaff = state.keys[GLFW_KEY_G];
			controller->super_speed = state.keys[GLFW_KEY_B];
			if (state.key_down[GLFW_KEY_F]) {
				selected_weapon = (selected_weapon + 1) % 3;
				visualizer->UpdateHudIcon(
					1,
					{1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, selected_weapon == 0}
				);
				visualizer->UpdateHudIcon(
					2,
					{2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, selected_weapon == 1}
				);
				visualizer->UpdateHudIcon(
					3,
					{3,
				     "assets/bullet-icon.png",
				     HudAlignment::TOP_LEFT,
				     {84 + 10 + 64, 10},
				     {64, 64},
				     selected_weapon == 2}
				);
			}
		});

		std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
		visualizer->AddShapeHandler([&](float time) { return shapes; });
		auto model = std::make_shared<Boidsish::Model>("assets/utah_teapot.obj");
		model->SetColossal(true);
		shapes.push_back(model);

		visualizer->GetAudioManager().PlayMusic("assets/kazoom.mp3", true, 0.25f);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
