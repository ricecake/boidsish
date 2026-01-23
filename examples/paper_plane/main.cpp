#include <iostream>
#include <memory>
#include <vector>

#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "PaperPlaneInputController.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include "ui/CoordinateWidget.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_projection.hpp>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Paper Plane Demo");
		visualizer->AddHudIcon(
			{1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, selected_weapon == 0}
		);
		visualizer->AddHudIcon(
			{2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, selected_weapon == 1}
		);
		visualizer->AddHudIcon(
			{3, "assets/bullet-icon.png", HudAlignment::TOP_LEFT, {84 + 10 + 64, 10}, {64, 64}, selected_weapon == 2}
		);

		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);
		plane->SetPosition(0, height + 50, 0);
		// visualizer->GetTerrainGenerator()->Raycast();
		Boidsish::Camera camera(0.0f, height + 55, -10.0f);
		visualizer->SetCamera(camera);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(plane);

		visualizer->AddHudGauge({3, 100.0f, "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

		auto controller = std::make_shared<PaperPlaneInputController>();
		std::dynamic_pointer_cast<PaperPlane>(plane)->SetController(controller);

		auto coordinate_widget = std::make_shared<UI::CoordinateWidget>();
		visualizer->AddWidget(coordinate_widget);

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			if (state.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
				int width, height;
				glfwGetWindowSize(visualizer->GetWindow(), &width, &height);

				glm::vec3 screen_pos(state.mouse_x, height - state.mouse_y, 0.0f);

				glm::vec4 viewport(0.0f, 0.0f, width, height);

				glm::vec3 world_pos = glm::unProject(
					screen_pos,
					visualizer->GetViewMatrix(),
					visualizer->GetProjectionMatrix(),
					viewport
				);

				const auto& cam = visualizer->GetCamera();
				glm::vec3 ray_origin = cam.pos();

				screen_pos.z = 1.0f;
				glm::vec3 far_plane_pos = glm::unProject(
					screen_pos,
					visualizer->GetViewMatrix(),
					visualizer->GetProjectionMatrix(),
					viewport
				);

				glm::vec3 ray_dir = glm::normalize(far_plane_pos - ray_origin);

				float distance;
				if (visualizer->GetTerrainGenerator()->Raycast(ray_origin, ray_dir, 1000.0f, distance)) {
					glm::vec3 intersection_point = ray_origin + ray_dir * distance;
					coordinate_widget->SetWorldPosition(intersection_point);
				}
			}

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
