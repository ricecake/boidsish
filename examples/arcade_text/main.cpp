#include <iostream>
#include <memory>
#include <vector>

#include "arcade_text.h"
#include "graphics.h"
#include <GLFW/glfw3.h>

int main() {
	Boidsish::Visualizer visualizer(1280, 720, "Arcade Text Effects Showcase");

	Boidsish::Camera camera;
	camera.x = 0.0f;
	camera.y = 20.0f;
	camera.z = 60.0f;
	camera.pitch = -10.0f;
	camera.yaw = 0.0f;
	visualizer.SetCamera(camera);

	// Add some light
	auto& light_manager = visualizer.GetLightManager();
	light_manager.AddLight(
		Boidsish::Light::CreateDirectional(
			glm::vec3(10.0f, 20.0f, 10.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			1.0f,
			glm::vec3(1.0f, 1.0f, 1.0f)
		)
	);
	light_manager.SetAmbientLight(glm::vec3(0.4f));

	// Helper to spawn different effects
	auto spawn_effects = [&]() {
		// 1. Vertical Rippling Wave + Rainbow
		auto t1 = visualizer.AddArcadeTextEffect(
			"VERTICAL WAVE",
			glm::vec3(-30, 30, 0),
			10.0f,
			45.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 1),
			10.0f
		);
		t1->SetWaveMode(Boidsish::ArcadeWaveMode::VERTICAL);
		t1->SetRainbowEnabled(true);
		t1->SetColor(1.0f, 0.5f, 0.0f);

		// 2. Flag Style Wave
		auto t2 = visualizer.AddArcadeTextEffect(
			"FLAG WAVE",
			glm::vec3(0, 30, 0),
			10.0f,
			45.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 1),
			10.0f
		);
		t2->SetWaveMode(Boidsish::ArcadeWaveMode::FLAG);
		t2->SetColor(0.0f, 1.0f, 0.5f);

		// 3. Lengthwise Twist
		auto t3 = visualizer.AddArcadeTextEffect(
			"TWISTED TEXT",
			glm::vec3(30, 30, 0),
			10.0f,
			45.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 1),
			10.0f
		);
		t3->SetWaveMode(Boidsish::ArcadeWaveMode::TWIST);
		t3->SetWaveAmplitude(1.0f);
		t3->SetColor(1.0f, 0.0f, 1.0f);

		// 4. Double Copy Rotating
		auto t4 = visualizer.AddArcadeTextEffect(
			"DOUBLE ROTATE",
			glm::vec3(0, 10, 0),
			15.0f,
			150.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, -1, 0),
			10.0f,
			"assets/Roboto-Medium.ttf",
			12.0f,
			5.0f
		);
		t4->SetDoubleCopy(true);
		t4->SetRotationSpeed(1.0f);
		t4->SetRotationAxis(glm::vec3(0, 1, 0));
		t4->SetRainbowEnabled(true);
		t4->SetColor(1.0f, 1.0f, 1.0f);

		// 5. Pulsing Scale + Rainbow
		auto t5 = visualizer.AddArcadeTextEffect(
			"PULSING ARCADE",
			glm::vec3(0, 10, 0),
			20.0f,
			60.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 1),
			10.0f
		);
		t5->SetPulseSpeed(3.0f);
		t5->SetPulseAmplitude(0.3f);
		t5->SetRainbowEnabled(true);
		t5->SetRainbowSpeed(5.0f);

		// 6. Bouncing Text
		auto t6 = visualizer.AddArcadeTextEffect(
			"BOUNCING!",
			glm::vec3(0, 30, 0),
			25.0f,
			45.0f,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 1),
			10.0f
		);
		t6->SetBounceSpeed(4.0f);
		t6->SetBounceAmplitude(5.0f);
		t6->SetColor(1.0f, 1.0f, 0.0f);
	};

	spawn_effects();

	visualizer.AddInputCallback([&](const Boidsish::InputState& state) {
		if (state.key_down[GLFW_KEY_SPACE]) {
			std::cout << "Respawning effects" << std::endl;
			spawn_effects();
		}
	});

	std::cout << "Arcade Text Effects Showcase" << std::endl;
	std::cout << "  Space: Respawn all effects" << std::endl;
	std::cout << "  WASD: Move camera" << std::endl;
	std::cout << "  ESC: Exit" << std::endl;

	visualizer.Run();

	return 0;
}
