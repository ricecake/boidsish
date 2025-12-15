#include <iostream>
#include <vector>
#include <functional>

#include "graphics.h"

// A wrapper to adapt the old ShapeFunction to the new ShapeHandler interface
class ShapeFunctionHandler : public Boidsish::ShapeHandler {
public:
    ShapeFunctionHandler(Boidsish::ShapeFunction func) : func_(func) {}

    const std::vector<std::shared_ptr<Boidsish::Shape>>& GetShapes(float time) override {
        shapes_ = func_(time);
        return shapes_;
    }

private:
    Boidsish::ShapeFunction                     func_;
    std::vector<std::shared_ptr<Boidsish::Shape>> shapes_;
};

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 10.0f;
		camera.z = 16.0f;
		camera.pitch = -30.0f;
		camera.yaw = -45.0f;
		visualizer.SetCamera(camera);

		// No shapes, just terrain
		visualizer.AddShapeHandler(std::make_shared<ShapeFunctionHandler>([](float /* time */) { return std::vector<std::shared_ptr<Boidsish::Shape>>(); }));

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}