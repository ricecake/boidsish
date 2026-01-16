#include <cmath>
#include <iostream>
#include <random>

#include "Config.h"
#include "ConfigManager.h"
#include "IWidget.h"
#include "dot.h"
#include "graphics.h"
#include "imgui.h"

using namespace Boidsish;

// Example 1: Spiraling particles
auto SpiralExample(float time) {
	std::vector<std::shared_ptr<Shape>> dots;

	int num_spirals = 3;
	int particles_per_spiral = 8;

	for (int spiral = 0; spiral < num_spirals; ++spiral) {
		for (int i = 0; i < particles_per_spiral; ++i) {
			float t = time * 0.3f + i * 0.2f;
			float angle = t + spiral * 2.0f * M_PI / num_spirals;
			float radius = 2.0f + t * 0.1f;
			float height = sin(t * 0.5f) * 3.0f;

			float x = cos(angle) * radius;
			float y = height + i * 0.3f;
			float z = sin(angle) * radius;

			// Color based on spiral and position
			float r = (spiral == 0) ? 1.0f : 0.3f;
			float g = (spiral == 1) ? 1.0f : 0.3f;
			float b = (spiral == 2) ? 1.0f : 0.3f;

			float size = 6.0f + 3.0f * sin(time + i * 0.5f);

			int dot_id = spiral * particles_per_spiral + i; // Unique ID for each dot
			dots.emplace_back(std::make_shared<Dot>(dot_id, x, y, z, size, r, g, b, 0.8f, 20));
		}
	}

	return dots;
}

// Example 2: Random walk particles
auto RandomWalkExample(float time) {
	static std::vector<std::tuple<float, float, float>> positions;
	static std::default_random_engine                   generator;
	static bool                                         initialized = false;

	if (!initialized) {
		positions.resize(10);
		for (auto& pos : positions) {
			pos = {0.0f, 0.0f, 0.0f};
		}
		initialized = true;
	}

	std::uniform_real_distribution<float> distribution(-0.1f, 0.1f);
	std::vector<std::shared_ptr<Shape>>   dots;

	for (size_t i = 0; i < positions.size(); ++i) {
		// Update position with random walk
		std::get<0>(positions[i]) += distribution(generator);
		std::get<1>(positions[i]) += distribution(generator);
		std::get<2>(positions[i]) += distribution(generator);

		// Boundary constraints
		for (int j = 0; j < 3; ++j) {
			float& coord = (j == 0) ? std::get<0>(positions[i])
				: (j == 1)          ? std::get<1>(positions[i])
									: std::get<2>(positions[i]);
			if (coord > 5.0f)
				coord = 5.0f;
			if (coord < -5.0f)
				coord = -5.0f;
		}

		float x = std::get<0>(positions[i]);
		float y = std::get<1>(positions[i]);
		float z = std::get<2>(positions[i]);

		// Color based on distance from origin
		float dist = sqrt(x * x + y * y + z * z);
		float r = 1.0f - dist / 8.0f;
		float g = dist / 8.0f;
		float b = 0.5f + 0.5f * sin(time + i);

		dots.emplace_back(std::make_shared<Dot>(static_cast<int>(i), x, y, z, 8.0f, r, g, b, 0.9f, 30));
	}

	return dots;
}

// Example 3: Wave function
auto WaveExample(float time) {
	std::vector<std::shared_ptr<Shape>> dots;

	int   grid_size = 15;
	float spacing = 0.5f;

	for (int i = 0; i < grid_size; ++i) {
		for (int j = 0; j < grid_size; ++j) {
			float x = (i - grid_size / 2) * spacing;
			float z = (j - grid_size / 2) * spacing;

			float dist = sqrt(x * x + z * z);
			float y = sin(dist * 0.8f - time * 2.0f) * 1.5f * exp(-dist * 0.1f);

			// Color based on height
			float r = 0.5f + 0.5f * (y / 1.5f);
			float g = 0.3f;
			float b = 1.0f - r;

			float size = 4.0f + 2.0f * (y / 1.5f);

			int dot_id = i * grid_size + j; // Unique ID based on grid position
			dots.emplace_back(std::make_shared<Dot>(dot_id, x, y, z, size, r, g, b, 0.7f, 5));
		}
	}

	return dots;
}

class InfoWidget: public UI::IWidget {
public:
	InfoWidget(Visualizer& viz, int example): m_viz(viz), m_example(example) {}

	void Draw() override {
		ImGui::Begin("Info");
		ImGui::Text("Current example: %d", m_example);
		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD - Move camera horizontally");
		ImGui::Text("  Space/Shift - Move camera up/down");
		ImGui::Text("  Mouse - Look around");
		ImGui::Text("  ESC - Exit");

		bool auto_camera = Boidsish::ConfigManager::GetInstance().GetAppSettingBool("auto_camera", true);
		if (ImGui::Checkbox("Auto Camera", &auto_camera)) {
			Boidsish::ConfigManager::GetInstance().SetBool("auto_camera", auto_camera);
			m_viz.SetCameraMode(auto_camera ? CameraMode::AUTO : CameraMode::FREE);
		}

		ImGui::End();
	}

private:
	Visualizer& m_viz;
	int         m_example;
};

int main(int argc, char* argv[]) {
	int example = 1;

	if (argc > 1) {
		example = std::atoi(argv[1]);
		if (example < 1 || example > 3) {
			std::cout << "Invalid example number. Using example 1." << std::endl;
			example = 1;
		}
	}

	try {
		std::string title = "Boidsish - Example " + std::to_string(example) + " - ";
		Visualizer  viz(1200, 800, title.c_str());

		// Set camera based on example
		Camera camera;
		if (example == 3) {
			camera = Camera(0.0f, 8.0f, 8.0f, -45.0f, 0.0f, 45.0f);
		} else {
			camera = Camera(0.0f, 2.0f, 10.0f, -10.0f, 0.0f, 45.0f);
		}
		viz.SetCamera(camera);

		switch (example) {
		case 1:
			title += "Spiraling Particles";
			viz.SetDotFunction(SpiralExample);
			break;
		case 2:
			title += "Random Walk";
			viz.SetDotFunction(RandomWalkExample);
			break;
		case 3:
			title += "Wave Function";
			viz.SetDotFunction(WaveExample);
			break;
		}

		viz.AddWidget(std::make_shared<InfoWidget>(viz, example));

		bool auto_camera = Boidsish::ConfigManager::GetInstance().GetAppSettingBool("auto_camera", true);
		viz.SetCameraMode(auto_camera ? CameraMode::AUTO : CameraMode::FREE);

		viz.SetMenusVisible(true);
		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}