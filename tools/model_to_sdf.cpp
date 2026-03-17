#include <iostream>
#include <string>
#include <glm/glm.hpp>
#include "asset_manager.h"
#include "model.h"
#include "sdf_utils.h"
#include "logger.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <model_path> <output_sdf_path> [resolution]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string output_path = argv[2];
    uint32_t resolution = 64;
    if (argc >= 4) {
        resolution = std::stoi(argv[3]);
    }

    // Assets might attempt GL calls, but they are guarded or should be safe if context is null.

    logger::INFO("Loading model: {}", model_path);
    auto model_data = AssetManager::GetInstance().GetModelData(model_path);
    if (!model_data) {
        logger::ERROR("Failed to load model data");
        return 1;
    }

    Model model(model_data);

    logger::INFO("Voxelizing model at resolution {}...", resolution);
    auto grid = SdfUtils::VoxelizeModel(model, glm::uvec3(resolution));

    logger::INFO("Generating SDF...");
    auto sdf = SdfUtils::GenerateSDF(grid);

    logger::INFO("Saving SDF to: {}", output_path);
    if (!SdfUtils::SaveSDF(sdf, output_path)) {
        logger::ERROR("Failed to save SDF");
        return 1;
    }

    logger::INFO("Done!");

    return 0;
}
