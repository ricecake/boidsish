#include <iostream>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "field.h"
#include "graphics.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

struct TerrainPoint {
	glm::vec3 position;
	glm::vec3 normal;
};

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 10.0f;
		camera.z = 16.0f;
		camera.pitch = 0.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		// No shapes, just terrain
		visualizer.AddShapeHandler([&](float) {
			auto cam = visualizer.GetCamera();
			auto shapes = std::vector<std::shared_ptr<Boidsish::Shape>>();
			auto dot = std::make_shared<Dot>(1);
			auto front = cam.front();
			auto dotPost = cam.pos() + 3.0f * cam.front();

			// dot->SetSize(15);
			dot->SetPosition(dotPost.x, dotPost.y, dotPost.z);
			dot->SetTrailLength(0); // Enable trails
			dot->SetColor(1.0f, 0.5f, 0.0f);
			shapes.push_back(dot);

			auto arrow2 = std::make_shared<Boidsish::Arrow>(1, 0, 0, 0, 0.1f, 0.1f, 0.01f, 0.0f, 1.0f, 0.0f);
			arrow2->SetPosition(dotPost.x, dotPost.y, dotPost.z);
			arrow2->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));

			// Sample terrain points around the arrow
			std::vector<TerrainPoint> terrain_points;
			const int grid_size = 10;
			const float spacing = 2.0f;
			for (int i = -grid_size / 2; i < grid_size / 2; ++i) {
				for (int j = -grid_size / 2; j < grid_size / 2; ++j) {
					float x = dotPost.x + i * spacing;
					float z = dotPost.z + j * spacing;
					auto [height, normal] = visualizer.GetTerrainPointProperties(x, z);
					terrain_points.push_back({{x, height, z}, normal});
				}
			}

			DivergenceFreePolicy policy(20.0f);
			glm::vec3            influence =
				CalculateField(glm::vec3(dotPost.x, dotPost.y, dotPost.z), terrain_points.begin(), terrain_points.end(), policy);

			if (glm::length(influence) > 0.001f) {
				influence = glm::normalize(influence);
				glm::vec3 up(0.0f, 1.0f, 0.0f);
				glm::vec3 axis = glm::cross(up, influence);
				float     angle = acos(glm::dot(up, influence));
				glm::quat rotation = glm::angleAxis(angle, axis);
				arrow2->SetRotation(rotation);
			}

			shapes.push_back(arrow2);

			return shapes;
		});

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
