#include <iostream>
#include <memory>
#include <string>

#include "ConfigManager.h"
#include "asset_manager.h"
#include "logger.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "Usage: " << argv[0] << " <input_model> <output_model> [simplify_ratio]" << std::endl;
		return 1;
	}

	std::string inputPath = argv[1];
	std::string outputPath = argv[2];
	float       ratio = 1.0f;
	if (argc >= 4) {
		ratio = std::stof(argv[3]);
	}

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return 1;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(640, 480, "Headless", NULL, NULL);
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

	auto& config = ConfigManager::GetInstance();
	config.SetBool("mesh_optimizer_enabled", true);
	if (ratio < 1.0f) {
		config.SetBool("mesh_simplifier_enabled", true);
		config.SetFloat("mesh_simplifier_target_ratio", ratio);
		config.SetFloat("mesh_simplifier_error_prebuild", 0.01f);
	} else {
		config.SetBool("mesh_simplifier_enabled", false);
	}

	auto modelData = AssetManager::GetInstance().GetModelData(inputPath);
	if (!modelData) {
		logger::ERROR("Failed to load model: {}", inputPath);
		return 1;
	}

	if (AssetManager::GetInstance().SaveModelData(modelData, outputPath)) {
		logger::LOG("Successfully converted {} to {}", inputPath, outputPath);
	} else {
		logger::ERROR("Failed to save model: {}", outputPath);
		return 1;
	}

	glfwTerminate();
	return 0;
}
