#include <cmath>
#include <memory>
#include <vector>

#include "graphics.h"
#include "model.h"
#include "text.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

int main(int argc, char** argv) {
	Boidsish::Visualizer vis(1280, 960, "Colossal Object Test - Orbital");

	std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

	// Colossal Teapot
	auto model = std::make_shared<Boidsish::Model>("assets/utah_teapot.obj");
	model->SetColossal(true);
	model->SetScale(glm::vec3(50.0f)); // Larger for orbital feel
	shapes.push_back(model);

	// Colossal Text
	auto text = std::make_shared<Boidsish::Text>("ORBITAL", "assets/fonts/Roboto-Bold.ttf", 48.0f, 1.0f);
	text->SetColossal(true);
	text->SetScale(glm::vec3(2.0f));
	shapes.push_back(text);

	vis.AddShapeHandler([&](float time) {
		// Move objects in a large circle in the sky
		float dist = 500.0f;
		float angle = time * 0.1f;

		float x = cos(angle) * dist;
		float z = sin(angle) * dist;
		float y = 150.0f + sin(time * 0.3f) * 50.0f;

		model->SetPosition(x, y, z);
		// Face the camera (origin in rotation-only sky view)
		model->SetRotation(glm::quatLookAt(glm::normalize(glm::vec3(-x, -y, -z)), glm::vec3(0, 1, 0)));

		text->SetPosition(-x, y + 100.0f, -z);
		text->SetRotation(glm::quatLookAt(glm::normalize(glm::vec3(x, -(y + 100.0f), z)), glm::vec3(0, 1, 0)));

		return shapes;
	});

	vis.Run();

	return 0;
}
