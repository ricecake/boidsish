#include <chrono>
#include <glm/glm.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "graphics.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>
#include <GL/glew.h>

using namespace Boidsish;

int main() {
	// Initialize GLFW for a headless context
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return 1;
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(640, 480, "Invisible Window", NULL, NULL);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return 1;
	}

	TerrainGenerator generator(12345);

	// Simulate some chunks being visible
	Frustum frustum;
	// Initialize planes for frustum to contain a reasonable area
	for (int i = 0; i < 6; ++i) {
		frustum.planes[i].normal = glm::vec3(0, 1, 0);
		frustum.planes[i].distance = 10000.0f;
	}

	Camera camera(0.0f, 100.0f, 0.0f);

	// Trigger chunk generation and wait for some to be ready
	std::cout << "Pre-warming terrain chunks..." << std::endl;
	for (int i = 0; i < 100; ++i) {
		generator.update(frustum, camera);
		if (generator.getVisibleChunks().size() > 5)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	auto visible_chunks = generator.getVisibleChunks();
	std::cout << "Visible chunks: " << visible_chunks.size() << std::endl;

	if (visible_chunks.empty()) {
		std::cout << "Warning: No terrain chunks ready for testing. Results may be trivial." << std::endl;
	}

	const int num_tests = 1000;
	int       hits = 0;

	std::cout << "Performing " << num_tests << " Octree-optimized raycasts..." << std::endl;
	auto start_ray = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < num_tests; ++i) {
		float     tx = (float)(i % 20 - 10) * 2.0f;
		float     tz = (float)(i / 20 - 25) * 2.0f;
		glm::vec3 origin(tx, 500.0f, tz);
		glm::vec3 dir(0.0f, -1.0f, 0.0f); // Top-down raycast
		float     dist;
		if (generator.Raycast(origin, dir, 1000.0f, dist)) {
			hits++;
		}
	}
	auto end_ray = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_ray - start_ray).count();

	std::cout << "Finished " << num_tests << " raycasts in " << duration << " us." << std::endl;
	std::cout << "Average time: " << (float)duration / num_tests << " us per raycast." << std::endl;
	std::cout << "Hits: " << hits << " / " << num_tests << std::endl;

	return 0;
}
