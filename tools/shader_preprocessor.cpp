#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "shader.h"
#include "constants.h"

/**
 * @brief Preprocessor class that exposes loadShaderSource from ShaderBase.
 */
class Preprocessor : public ShaderBase {
public:
    std::string process(const std::string& path, std::set<std::string>& includedFiles) {
        return loadShaderSource(path, includedFiles);
    }
};

int main(int argc, char** argv) {
    std::string inputPath;
    std::string outputPath;
    std::string depfilePath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--depfile" && i + 1 < argc) {
            depfilePath = argv[++i];
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else if (outputPath.empty()) {
            outputPath = arg;
        }
    }

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: " << argv[0] << " [--depfile <depfile>] <input_shader> <output_file>" << std::endl;
        return 1;
    }

    // Register constants (synchronized with src/graphics.cpp)
    ShaderBase::RegisterConstant("MAX_LIGHTS", Boidsish::Constants::Class::Shadows::MaxLights());
    ShaderBase::RegisterConstant("MAX_SHADOW_MAPS", Boidsish::Constants::Class::Shadows::MaxShadowMaps());
    ShaderBase::RegisterConstant("MAX_CASCADES", Boidsish::Constants::Class::Shadows::MaxCascades());
    ShaderBase::RegisterConstant("CHUNK_SIZE", Boidsish::Constants::Class::Terrain::ChunkSize());
    ShaderBase::RegisterConstant("CHUNK_SIZE_PLUS_1", Boidsish::Constants::Class::Terrain::ChunkSizePlus1());
    ShaderBase::RegisterConstant("MAX_SHOCKWAVES", Boidsish::Constants::Class::Shockwaves::MaxShockwaves());

    Preprocessor p;
    std::set<std::string> includedFiles;
    std::string processed = p.process(inputPath, includedFiles);

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

    // Write dependency file if requested
    if (!depfilePath.empty()) {
        std::ofstream dep(depfilePath);
        if (dep.is_open()) {
            dep << outputPath << ":";
            for (const auto& file : includedFiles) {
                // The input file itself is also a dependency
                dep << " " << file;
            }
            dep << std::endl;
            dep.close();
        } else {
            std::cerr << "Warning: Failed to open depfile " << depfilePath << std::endl;
        }
    }

    return 0;
}
