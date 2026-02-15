#include "graphics.h"
#include "dot.h"
#include "hud.h"
#include "line.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>

#include <iomanip>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>

using namespace Boidsish;

int main() {
	try {
		Visualizer visualizer(1280, 720, "Closest Terrain Visualizer");
		visualizer.SetCameraMode(CameraMode::TRACKING);

		auto terrain_gen = visualizer.GetTerrain();
		if (terrain_gen) {
			terrain_gen->SetWorldScale(2.0f);
		}

		glm::vec3 probe_pos(0, 100, 0);
		float     cone_spread = 0.5f;
		bool      spherical_mode = true;

		// Initial camera setup
		Camera& cam = visualizer.GetCamera();
		cam.x = 100.0f;
		cam.y = 150.0f;
		cam.z = 100.0f;

		// We'll use these to render
		auto probe_dot = std::make_shared<Dot>(999, 0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f, 0.0f);
		auto result_line = std::make_shared<Line>(glm::vec3(0), glm::vec3(0), 0.5f);
		result_line->SetStyle(Line::Style::LASER);
		result_line->SetColor(0.0f, 1.0f, 1.0f, 0.8f);

		auto msg = visualizer.AddHudMessage("Mode: Spherical", HudAlignment::TOP_CENTER, {0, 20}, 1.0f);

		visualizer.AddInputCallback([&](const InputState& input) {
			float speed = 100.0f * input.delta_time;

			// Move relative to world axes
			if (input.keys[GLFW_KEY_W])
				probe_pos.z -= speed;
			if (input.keys[GLFW_KEY_S])
				probe_pos.z += speed;
			if (input.keys[GLFW_KEY_A])
				probe_pos.x -= speed;
			if (input.keys[GLFW_KEY_D])
				probe_pos.x += speed;
			if (input.keys[GLFW_KEY_LEFT_SHIFT])
				probe_pos.y += speed;
			if (input.keys[GLFW_KEY_LEFT_CONTROL])
				probe_pos.y -= speed;

			// Toggle mode
			if (input.key_down[GLFW_KEY_M]) {
				spherical_mode = !spherical_mode;
			}

			// Control spread
			if (input.keys[GLFW_KEY_E])
				cone_spread += 2.0f * input.delta_time;
			if (input.keys[GLFW_KEY_Q])
				cone_spread = std::max(0.01f, cone_spread - 2.0f * input.delta_time);

			std::string mode_str =
				spherical_mode ? "Spherical" : "Conical (Spread: " + std::to_string(cone_spread) + ")";
			msg->SetMessage("Mode: " + mode_str + "\nWASD: Move, Shift/Ctrl: Height, M: Toggle Mode, Q/E: Spread");
		});

		visualizer.AddShapeHandler([&](float time) {
			(void)time;
			auto                                terrain = visualizer.GetTerrain();
			std::vector<std::shared_ptr<Shape>> shapes;
			if (!terrain)
				return shapes;

			std::tuple<float, glm::vec3> result;
			if (spherical_mode) {
				result = terrain->GetClosestTerrain(probe_pos);
			} else {
				// Search direction is camera front (opposite camera from focus point)
				glm::vec3 cone_dir = visualizer.GetCamera().front();
				result = terrain->GetClosestTerrain(probe_pos, cone_spread, cone_dir);
			}

			float     dist = std::get<0>(result);
			glm::vec3 dir = std::get<1>(result);
			glm::vec3 target = probe_pos + dir * dist;

			probe_dot->SetPosition(probe_pos.x, probe_pos.y, probe_pos.z);
			result_line->SetStart(probe_pos);
			result_line->SetEnd(target);

			// First shape is the one tracked by TRACKING mode
			shapes.push_back(probe_dot);
			shapes.push_back(result_line);

			return shapes;
		});

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
