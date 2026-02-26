#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "walking_creature.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main(int argc, char** argv) {
	try {
		Visualizer vis(1280, 960, "Walking Creature Demo");

		// Create the walking creature at the origin
		auto creature = std::make_shared<WalkingCreature>(0, 0, 0, 0, 4.0f);
		creature->SetClampedToTerrain(true);

		// Use a shape handler to update and return the creature for rendering
		vis.AddShapeHandler([&creature, &vis](float time) {
			creature->SetCameraPosition(vis.GetCamera().pos());
			creature->Update(vis.GetLastFrameTime());
			return std::vector<std::shared_ptr<Shape>>{creature};
		});

		// Add an input callback to set the creature's target when the left mouse button is clicked
		vis.AddInputCallback([&](const InputState& state) {
			if (state.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT]) {
				auto world_pos = vis.ScreenToWorld(state.mouse_x, state.mouse_y);
				if (world_pos) {
					creature->SetTarget(*world_pos);
				}
			}
		});

		std::cout << "Left click on the terrain to make the creature walk!" << std::endl;

		vis.SetCameraMode(CameraMode::AUTO);
		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
