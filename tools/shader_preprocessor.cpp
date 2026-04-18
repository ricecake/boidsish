#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "shader.h"
#include "constants.h"
#include "shader_registration.h"

/**
 * @brief Preprocessor class that exposes loadShaderSource from ShaderBase.
 */
class Preprocessor : public ShaderBase {
public:
    std::string process(const std::string& path) {
        std::set<std::string> includedFiles;
        return loadShaderSource(path, includedFiles);
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_shader> <output_file>" << std::endl;
        return 1;
    }

    Boidsish::RegisterAllShaderConstants<ShaderBase>();

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    Preprocessor p;
    std::string processed = p.process(inputPath);

    if (processed.empty()) {
        std::cerr << "Error: Failed to preprocess " << inputPath << std::endl;
        return 1;
    }

    // Ensure output directory exists
    std::filesystem::path outPath(outputPath);
    if (outPath.has_parent_path()) {
        std::filesystem::create_directories(outPath.parent_path());
    }

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open output file " << outputPath << std::endl;
        return 1;
    }
    out << processed;
    out.close();

    return 0;
}
