#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"

std::vector<std::shared_ptr<Boidsish::Shape>> CreateShapes(float) {
	std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
	for (int i = 0; i < 5; ++i) {
		auto dot = std::make_shared<Boidsish::Dot>();
		dot->SetPosition(i * 2.0f - 4.0f, 0.0f, 0.0f);
		dot->SetColor(1.0f, 0.0f, 0.0f);
		shapes.push_back(dot);
	}
	return shapes;
}

int main() {
	Boidsish::Visualizer visualizer(1280, 720, "Artistic Effects Demo");
	visualizer.AddShapeHandler(CreateShapes);
	visualizer.Run();
	return 0;
}
