#include <iostream>
#include <memory>

#include "graphics.h"
#include "model.h"

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1024, 768, "Model Loading Example");

		viz.AddShapeHandler([&](float /* time */) {
			auto shapes = std::vector<std::shared_ptr<Shape>>();
			auto model = std::make_shared<Model>("assets/quad.obj", true);
			shapes.push_back(model);
			return shapes;
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
