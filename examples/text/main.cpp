#include <memory>

#include "graphics.h"
#include "text.h"
#include <GLFW/glfw3.h>

int main() {
	Boidsish::Visualizer visualizer(1280, 720, "Text Example");

	Boidsish::Camera camera;
	camera.x = 0.0f;
	camera.y = 10.0f;
	camera.z = 50.0f;
	camera.pitch = 0.0f;
	camera.yaw = 0.0f;
	visualizer.SetCamera(camera);

	auto text_shape = std::make_shared<Boidsish::Text>(
		"Hello, World!",
		"external/imgui/misc/fonts/Roboto-Medium.ttf",
		16.0f,
		2.0f,
		0,
		0.0f,
		10.0f,
		0.0f
	);
	text_shape->SetScale(glm::vec3(0.5f));
	visualizer.AddShape(text_shape);

    double last_time = glfwGetTime();
    int frame_count = 0;
    std::string original_text = "Hello, World!";

	while (!visualizer.ShouldClose()) {
		visualizer.Update();
        double current_time = glfwGetTime();
        frame_count++;
        if (current_time - last_time >= 1.0) {
            std::string new_text = original_text + " " + std::to_string(frame_count);
            text_shape->SetText(new_text);
            frame_count = 0;
            last_time = current_time;
        }
	}

	return 0;
}
