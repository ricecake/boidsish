#include <iostream>
#include <memory>
#include <algorithm>

#include <GL/glew.h>
#include "graphics.h"
#include "IWidget.h"
#include "ui/CompetitiveCAWidget.h"

using namespace Boidsish;

int main() {
	try {
		// Initialize the visualizer window
		Visualizer viz(1280, 720, "Competitive Cellular Automata Texture Synthesizer");

		// Set a stationary camera looking at the center
		Camera camera(0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f);
		viz.SetCamera(camera);

		// Instantiating our widget and adding it to the visualizer UI
		// Since we already registered CompetitiveCAWidget in UIConfigManager,
		// it will show up by default, but having a dedicated widget instance added
		// in this standalone example ensures the menu is focused.
		auto ca_widget = std::make_shared<UI::CompetitiveCAWidget>(viz);
		viz.AddWidget(ca_widget);

		// Run the visualization main loop
		std::cout << "Starting Competitive Cellular Automata Demo..." << std::endl;
		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Fatal Exception in Competitive CA Demo: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
