#include <iostream>
#include <vector>
#include <cmath>

#include "graphics.h"

std::vector<glm::vec4> generateSphere(int num_points, float radius) {
    std::vector<glm::vec4> points;
    for (int i = 0; i < num_points; ++i) {
        float u = static_cast<float>(rand()) / RAND_MAX;
        float v = static_cast<float>(rand()) / RAND_MAX;
        float theta = 2.0f * M_PI * u;
        float phi = acos(2.0f * v - 1.0f);
        float x = radius * sin(phi) * cos(theta);
        float y = radius * sin(phi) * sin(theta);
        float z = radius * cos(phi);
        float value = static_cast<float>(rand()) / RAND_MAX; // Random value for thresholding
        points.emplace_back(x, y, z, value);
    }
    return points;
}

int main() {
    try {
        Boidsish::Visualizer visualizer(1280, 720, "Point Cloud Demo");

        Boidsish::Camera camera;
        camera.x = 0.0f;
        camera.y = 0.0f;
        camera.z = 50.0f;
        visualizer.SetCamera(camera);

        auto point_data = generateSphere(100000, 20.0f);
        visualizer.SetPointCloudData(point_data);
        visualizer.SetPointCloudThreshold(0.5f);
        visualizer.SetPointCloudSize(2.0f);

        // No other shapes
        visualizer.AddShapeHandler([](float /* time */) { return std::vector<std::shared_ptr<Boidsish::Shape>>(); });

        visualizer.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
