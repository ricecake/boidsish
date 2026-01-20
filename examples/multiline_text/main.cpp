#include <memory>
#include <vector>

#include "graphics.h"
#include "text.h"
#include <GLFW/glfw3.h>

int main() {
	Boidsish::Visualizer visualizer(1280, 720, "Multiline Text Example");

	auto left_text = std::make_shared<Boidsish::Text>(
		"This is a\nleft-justified\ntext block.",
		"assets/Roboto-Medium.ttf",
		24.0f,
		5.0f,
		Boidsish::Text::LEFT,
		0,
		0,
		30,
		0
	);

	auto center_text = std::make_shared<Boidsish::Text>(
		"This is a\ncentered\ntext block.",
		"assets/Roboto-Medium.ttf",
		24.0f,
		5.0f,
		Boidsish::Text::CENTER,
		0,
		0,
		30,
		30
	);

	auto right_text = std::make_shared<Boidsish::Text>(
		"This is a\nright-justified\ntext block.",
		"assets/Roboto-Medium.ttf",
		24.0f,
		5.0f,
		Boidsish::Text::RIGHT,
		0,
		0,
		30,
		60
	);

	visualizer.AddShape(left_text);
	visualizer.AddShape(center_text);
	visualizer.AddShape(right_text);

	double start_time = glfwGetTime();
	bool   justification_changed = false;

	visualizer.AddShapeHandler([&](float delta_time) -> std::vector<std::shared_ptr<Boidsish::Shape>> {
		double current_time = glfwGetTime();
		if (current_time - start_time > 3.0f && !justification_changed) {
			center_text->SetJustification(Boidsish::Text::LEFT);
			justification_changed = true;
		}
		return {left_text, center_text, right_text};
	});

	visualizer.Run();

	return 0;
}
